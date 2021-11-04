#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define MBEDTLS_CONFIG_FILE "mbedConfig.h"
#include <mbedtls/pk.h>

#define STEAM_API_NODLL
#include <steam/steam_api.h>
#include <steam/isteamappticket.h>
#include "SteamCert.h"

#include "xorstr.h"

typedef bool (S_CALLTYPE* OrgSteamAPI_Init)();
OrgSteamAPI_Init oSteamAPI_Init = nullptr;

S_API bool S_CALLTYPE SteamAPI_Init()
{
	return oSteamAPI_Init();
}

typedef bool (S_CALLTYPE* OrgSteamAPI_RestartAppIfNecessary)(uint32 unOwnAppID);
OrgSteamAPI_RestartAppIfNecessary oSteamAPI_RestartAppIfNecessary = nullptr;

S_API bool S_CALLTYPE SteamAPI_RestartAppIfNecessary(uint32 unOwnAppID)
{
	return oSteamAPI_RestartAppIfNecessary(unOwnAppID);
}

typedef void* (S_CALLTYPE* OrgSteamInternal_ContextInit)(void* pContextInitData);
OrgSteamInternal_ContextInit oSteamInternal_ContextInit = nullptr;

S_API void* S_CALLTYPE SteamInternal_ContextInit(void* pContextInitData)
{
	return oSteamInternal_ContextInit(pContextInitData);
}

typedef void* (S_CALLTYPE* OrgSteamInternal_CreateInterface)(const char* ver);
OrgSteamInternal_CreateInterface oSteamInternal_CreateInterface = nullptr;

S_API void* S_CALLTYPE SteamInternal_CreateInterface(const char* ver)
{
	return oSteamInternal_CreateInterface(ver);
}

typedef HSteamPipe(S_CALLTYPE* OrgSteamAPI_GetHSteamPipe)();
OrgSteamAPI_GetHSteamPipe oSteamAPI_GetHSteamPipe;

S_API HSteamPipe S_CALLTYPE SteamAPI_GetHSteamPipe()
{
	return oSteamAPI_GetHSteamPipe();
}

typedef HSteamUser(S_CALLTYPE* OrgSteamAPI_GetHSteamUser)();
OrgSteamAPI_GetHSteamUser oSteamAPI_GetHSteamUser;

S_API HSteamUser S_CALLTYPE SteamAPI_GetHSteamUser()
{
	return oSteamAPI_GetHSteamUser();
}

bool InitSteamDll()
{
	const auto steamModule = LoadLibrary(xorstr_(L"steam_api64.dll"));

	if (steamModule == nullptr)
		return false;

	oSteamAPI_Init = reinterpret_cast<OrgSteamAPI_Init>(GetProcAddress(steamModule, xorstr_("SteamAPI_Init")));
	oSteamAPI_RestartAppIfNecessary = reinterpret_cast<OrgSteamAPI_RestartAppIfNecessary>(GetProcAddress(steamModule, xorstr_("SteamAPI_RestartAppIfNecessary")));
	oSteamAPI_GetHSteamUser = reinterpret_cast<OrgSteamAPI_GetHSteamUser>(GetProcAddress(steamModule, xorstr_("SteamAPI_GetHSteamUser")));
	oSteamAPI_GetHSteamPipe = reinterpret_cast<OrgSteamAPI_GetHSteamPipe>(GetProcAddress(steamModule, xorstr_("SteamAPI_GetHSteamPipe")));

	oSteamInternal_ContextInit = reinterpret_cast<OrgSteamInternal_ContextInit>(GetProcAddress(steamModule, xorstr_("SteamInternal_ContextInit")));
	oSteamInternal_CreateInterface = reinterpret_cast<OrgSteamInternal_CreateInterface>(GetProcAddress(steamModule, xorstr_("SteamInternal_CreateInterface")));

	if (
		oSteamAPI_Init == nullptr ||
		oSteamAPI_RestartAppIfNecessary == nullptr ||
		oSteamAPI_GetHSteamUser == nullptr ||
		oSteamAPI_GetHSteamPipe == nullptr ||
		oSteamInternal_ContextInit == nullptr ||
		oSteamInternal_CreateInterface == nullptr
		)
		return false;

	return true;
}

bool CheckSignature(const unsigned char* ticket, const uint32* ticketLen, const unsigned char* signature, const uint32* signatureLen)
{
	mbedtls_pk_context pk;
	mbedtls_pk_init(&pk);

	if (mbedtls_pk_parse_public_key(&pk, xorstr_(SteamCert), sizeof SteamCert))
	{
		mbedtls_pk_free(&pk);
		return false;
	}

	unsigned char hash[20];

	if (mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), ticket, *ticketLen, hash))
	{
		mbedtls_pk_free(&pk);
		return false;
	}

	if (mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA1, hash, sizeof(hash), signature, *signatureLen))
	{
		mbedtls_pk_free(&pk);
		return false;
	}
	mbedtls_pk_free(&pk);
	return true;
}

bool FetchSteamTicket(const uint32 appId, unsigned char* ticket, uint32* ticketLength, unsigned char** signature, uint32* signatureLength)
{
	if (SteamAPI_RestartAppIfNecessary(appId))
		return false;
	if (!SteamAPI_Init())
		return false;

	const auto steamClient = SteamClient();

	auto* steamAppTicket = static_cast<ISteamAppTicket*>(steamClient->GetISteamGenericInterface(
		SteamAPI_GetHSteamUser(), SteamAPI_GetHSteamPipe(), xorstr_(STEAMAPPTICKET_INTERFACE_VERSION)));

	uint32 iTicketAppId;
	uint32 iTicketSteamId;
	uint32 iTicketSignature;

	const uint32 ret = steamAppTicket->GetAppOwnershipTicketData(appId, ticket, *ticketLength, &iTicketAppId, &iTicketSteamId, &iTicketSignature, signatureLength);

	if (ret == 0)
	{
		return false;
	}

	AppId_t ticketAppId;
	memcpy(&ticketAppId, &ticket[iTicketAppId], sizeof(AppId_t));
	if (ticketAppId != appId)
	{
		return false;
	}

	CSteamID ticketSteamId;
	memcpy(&ticketSteamId, &ticket[iTicketSteamId], sizeof(CSteamID));
	const auto steamId = steamClient->GetISteamUser(SteamAPI_GetHSteamUser(), SteamAPI_GetHSteamPipe(), STEAMUSER_INTERFACE_VERSION)->GetSteamID();
	if (ticketSteamId != steamId)
	{
		return false;
	}

	*ticketLength = ret - *signatureLength;

	*signature = &ticket[iTicketSignature];
	return true;
}

bool CheckSteamActivation(const int appId)
{
	unsigned char ticketBuff[256];
	uint32 ticketLength = sizeof(ticketBuff);

	unsigned char* signatureBuff = nullptr;
	uint32 signatureLength = 128;

	if (!InitSteamDll())
		return false;

	if (!FetchSteamTicket(appId, ticketBuff, &ticketLength, &signatureBuff, &signatureLength))
		return false;

	return !CheckSignature(ticketBuff, &ticketLength, signatureBuff, &signatureLength) ? false : true;
}
