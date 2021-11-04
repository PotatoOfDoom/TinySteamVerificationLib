# Tiny Steam Verification Lib
Include this in your game/app to prevent basic DRM bypasses using Steam Emulators. This lib uses a method which is 100% official (just not very well-known), entirely offline, doesn't use any extra system resources, completely seamless for the end-user and impossible to circumvent without explicitly patching the executable code in the binary.

## How does this work
Steams API exposes "GetAppOwnershipTicketData", an API to get so-called App Ownership Tickets. Those tickets are signed by valve and are a proof of purchase for every steam game. Those tickets can't be forged and contain ownership data about the game and the steam user. Steam's own DRM solution uses this API, but third-party devs generally don't implement it, due to its bad documentation/explanation of what it actually does.https://partner.steamgames.com/doc/api/ISteamAppTicket

## Requirements
  * mbedtls in your project
  * a valid steam appId

## Usage
  1. Download this repo
  2. Download the steam sdk
  3. Drag it into /external/steamworksSDK/
  4. Open the SteamActivationLib.sln in VS 2019
  5. Select the appropriate compilation options for your game and compile it
  6. Integrate the resulting static library, the "SteamActivationLib.h" header file and [mbedtls](https://github.com/ARMmbed/mbedtls) into your game.
  7. Call `bool CheckSteamActivation(int appId)` whenever you want to check for the ownership of your game.  Bonus Points if you hide the api calls and break gameplay features of your game for pirates which aren't immediately noticeable.

## How to make this even more secure
  * Don't use the static library and copy the code directly into other unrelated game functions
  * Enable the always inline compiler flag
  * Don't immediately check the ownership in the beginning and check it when while running some other stuff (loading textures for example) or even randomize the checks - be creative ;-)
  * Use commercial obfuscators like VMProtect or Themida (or if you have the time: create your own)

## Possible improvements for the future
  * Remove the mbedtls dependency for something lighter.
	  * Note: It doesn't need to be super safe because it only verifies the ticket, so it can be a custom implementation.
  * Don't use the LoadLibrary/GetProcAddress calls and instead use the good old Process Environment Block trick to get the functions
	
## Credits
  * [mbedtls](https://github.com/ARMmbed/mbedtls)
  * [xorstr](https://github.com/JustasMasiulis/xorstr)
  * [SteamKit2](https://github.com/SteamRE/SteamKit)
