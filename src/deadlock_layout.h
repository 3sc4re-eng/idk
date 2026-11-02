#pragma once
#include <cstdint>
// Exact installed revision 11193905; generated from SchemaExplorer and local PE analysis.
namespace dl_layout{
namespace CSkeletonInstance{inline constexpr uint32_t m_modelState=320;}
namespace CModelState{inline constexpr uint32_t m_ModelName=168;}
namespace CCitadelAutoScaledTime{inline constexpr uint32_t m_flTime=8;} // Exact schema GameTime_t within the networked wrapper.
inline constexpr uint32_t globals_rva=0x346e730,globals_instruction=0x4c81a8;
inline constexpr uint32_t revision=11193905,client_timestamp=1793470523u,client_image_size=71352320u;
namespace C_BaseEntity{
inline constexpr uint32_t size=1520;
inline constexpr uint32_t m_CBodyComponent=48;
inline constexpr uint32_t m_NetworkTransmitComponent=56;
inline constexpr uint32_t m_nLastThinkTick=808;
inline constexpr uint32_t m_pGameSceneNode=816;
inline constexpr uint32_t m_pRenderComponent=824;
inline constexpr uint32_t m_pCollision=832;
inline constexpr uint32_t m_pModifierProp=840;
inline constexpr uint32_t m_iMaxHealth=848;
inline constexpr uint32_t m_iHealth=852;
inline constexpr uint32_t m_flDamageAccumulator=856;
inline constexpr uint32_t m_lifeState=860;
inline constexpr uint32_t m_bTakesDamage=861;
inline constexpr uint32_t m_nTakeDamageFlags=864;
inline constexpr uint32_t m_nPlatformType=872;
inline constexpr uint32_t m_ubInterpolationFrame=873;
inline constexpr uint32_t m_hSceneObjectController=876;
inline constexpr uint32_t m_nNoInterpolationTick=880;
inline constexpr uint32_t m_nVisibilityNoInterpolationTick=884;
inline constexpr uint32_t m_flProxyRandomValue=888;
inline constexpr uint32_t m_iEFlags=892;
inline constexpr uint32_t m_nWaterType=896;
inline constexpr uint32_t m_bInterpolateEvenWithNoModel=897;
inline constexpr uint32_t m_bPredictionEligible=898;
inline constexpr uint32_t m_bApplyLayerMatchIDToModel=899;
inline constexpr uint32_t m_tokLayerMatchID=900;
inline constexpr uint32_t m_nSubclassID=904;
inline constexpr uint32_t m_nSimulationTick=920;
inline constexpr uint32_t m_iCurrentThinkContext=924;
inline constexpr uint32_t m_aThinkFunctions=928;
inline constexpr uint32_t m_bDisabledContextThinks=952;
inline constexpr uint32_t m_flAnimTime=956;
inline constexpr uint32_t m_flSimulationTime=960;
inline constexpr uint32_t m_nSceneObjectOverrideFlags=964;
inline constexpr uint32_t m_bHasSuccessfullyInterpolated=965;
inline constexpr uint32_t m_bHasAddedVarsToInterpolation=966;
inline constexpr uint32_t m_bRenderEvenWhenNotSuccessfullyInterpolated=967;
inline constexpr uint32_t m_nInterpolationLatchDirtyFlags=968;
inline constexpr uint32_t m_ListEntry=976;
inline constexpr uint32_t m_flCreateTime=1000;
inline constexpr uint32_t m_EntClientFlags=1004;
inline constexpr uint32_t m_bClientSideRagdoll=1006;
inline constexpr uint32_t m_iTeamNum=1007;
inline constexpr uint32_t m_spawnflags=1008;
inline constexpr uint32_t m_nNextThinkTick=1012;
inline constexpr uint32_t m_fFlags=1020;
inline constexpr uint32_t m_vecAbsVelocity=1024;
inline constexpr uint32_t m_vecServerVelocity=1036;
inline constexpr uint32_t m_vecVelocity=1080;
inline constexpr uint32_t m_hEffectEntity=1304;
inline constexpr uint32_t m_hOwnerEntity=1308;
inline constexpr uint32_t m_MoveCollide=1312;
inline constexpr uint32_t m_MoveType=1313;
inline constexpr uint32_t m_nActualMoveType=1314;
inline constexpr uint32_t m_flWaterLevel=1316;
inline constexpr uint32_t m_fEffects=1320;
inline constexpr uint32_t m_hGroundEntity=1324;
inline constexpr uint32_t m_nGroundBodyIndex=1328;
inline constexpr uint32_t m_flFriction=1332;
inline constexpr uint32_t m_flElasticity=1336;
inline constexpr uint32_t m_flGravityScale=1340;
inline constexpr uint32_t m_flTimeScale=1344;
inline constexpr uint32_t m_bAnimatedEveryTick=1348;
inline constexpr uint32_t m_bGravityDisabled=1349;
inline constexpr uint32_t m_flNavIgnoreUntilTime=1352;
inline constexpr uint32_t m_hThink=1356;
inline constexpr uint32_t m_fBBoxVisFlags=1368;
inline constexpr uint32_t m_flActualGravityScale=1372;
inline constexpr uint32_t m_bGravityActuallyDisabled=1376;
inline constexpr uint32_t m_bPredictable=1377;
inline constexpr uint32_t m_bRenderWithViewModels=1378;
inline constexpr uint32_t m_nFirstPredictableCommand=1380;
inline constexpr uint32_t m_nLastPredictableCommand=1384;
inline constexpr uint32_t m_hOldMoveParent=1388;
inline constexpr uint32_t m_Particles=1392;
inline constexpr uint32_t m_vecAngVelocity=1440;
inline constexpr uint32_t m_DataChangeEventRef=1452;
inline constexpr uint32_t m_dependencies=1456;
inline constexpr uint32_t m_nCreationTick=1480;
inline constexpr uint32_t m_bAnimTimeChanged=1497;
inline constexpr uint32_t m_bSimulationTimeChanged=1498;
inline constexpr uint32_t m_sUniqueHammerID=1512;
}
namespace CEntityInstance{
inline constexpr uint32_t size=48;
inline constexpr uint32_t m_iszPrivateVScripts=8;
inline constexpr uint32_t m_pEntity=16;
inline constexpr uint32_t m_CScriptComponent=40;
}
namespace CGameSceneNode{
inline constexpr uint32_t size=304;
inline constexpr uint32_t m_nodeToWorld=16;
inline constexpr uint32_t m_pOwner=48;
inline constexpr uint32_t m_pParent=56;
inline constexpr uint32_t m_pChild=64;
inline constexpr uint32_t m_pNextSibling=72;
inline constexpr uint32_t m_hParent=112;
inline constexpr uint32_t m_vecOrigin=128;
inline constexpr uint32_t m_angRotation=184;
inline constexpr uint32_t m_flScale=196;
inline constexpr uint32_t m_vecAbsOrigin=200;
inline constexpr uint32_t m_angAbsRotation=212;
inline constexpr uint32_t m_flAbsScale=224;
inline constexpr uint32_t m_vecWrappedLocalOrigin=228;
inline constexpr uint32_t m_angWrappedLocalRotation=240;
inline constexpr uint32_t m_flWrappedScale=252;
inline constexpr uint32_t m_nParentAttachmentOrBone=256;
inline constexpr uint32_t m_bDebugAbsOriginChanges=258;
inline constexpr uint32_t m_bDormant=259;
inline constexpr uint32_t m_bForceParentToBeNetworked=260;
inline constexpr uint32_t m_nHierarchicalDepth=263;
inline constexpr uint32_t m_nHierarchyType=264;
inline constexpr uint32_t m_nDoNotSetAnimTimeInInvalidatePhysicsCount=265;
inline constexpr uint32_t m_name=268;
inline constexpr uint32_t m_hierarchyAttachName=288;
inline constexpr uint32_t m_flClientLocalScale=292;
}
namespace CBasePlayerController{
inline constexpr uint32_t size=2048;
inline constexpr uint32_t m_CommandContext=1528;
inline constexpr uint32_t m_nInButtonsWhichAreToggles=1712;
inline constexpr uint32_t m_nTickBase=1720;
inline constexpr uint32_t m_hPawn=1724;
inline constexpr uint32_t m_bKnownTeamMismatch=1728;
inline constexpr uint32_t m_hPredictedPawn=1732;
inline constexpr uint32_t m_nSplitScreenSlot=1740;
inline constexpr uint32_t m_hSplitOwner=1744;
inline constexpr uint32_t m_hSplitScreenPlayers=1752;
inline constexpr uint32_t m_bIsHLTV=1776;
inline constexpr uint32_t m_iConnected=1780;
inline constexpr uint32_t m_iMostConnected=1784;
inline constexpr uint32_t m_iszPlayerName=1788;
inline constexpr uint32_t m_steamID=1928;
inline constexpr uint32_t m_bIsLocalPlayerController=1936;
inline constexpr uint32_t m_bNoClipEnabled=1937;
inline constexpr uint32_t m_iDesiredFOV=1940;
}
namespace CCitadelPlayerController{
inline constexpr uint32_t size=3168;
inline constexpr uint32_t m_ePlayState=2048;
inline constexpr uint32_t m_iGuidedBotMatchLastHits=2052;
inline constexpr uint32_t m_iGuidedBotMatchOrbsSecured=2056;
inline constexpr uint32_t m_iGuidedBotMatchOrbsDenied=2060;
inline constexpr uint32_t m_iGuidedBotMatchDamageToGuardians=2064;
inline constexpr uint32_t m_iGuidedBotMatchDamageToPlayers=2068;
inline constexpr uint32_t m_iGuidedBotMatchDamageTaken=2072;
inline constexpr uint32_t m_iGuidedBotMatchNetWorth=2076;
inline constexpr uint32_t m_iGuidedBotMatchModsPurchased=2080;
inline constexpr uint32_t m_iGuidedBotMatchAbilityUpgrades=2084;
inline constexpr uint32_t m_flGuideBotMatchLastTaskNagVO=2088;
inline constexpr uint32_t m_flGuideBotLastTimeTaskCompleted=2092;
inline constexpr uint32_t m_eGuidedBotMatchObjective=2096;
inline constexpr uint32_t m_nCurrentRank=2100;
inline constexpr uint32_t m_nAssignedLane=2104;
inline constexpr uint32_t m_nOriginalLaneAssignment=2105;
inline constexpr uint32_t m_bBotDisconnectTakeover=2106;
inline constexpr uint32_t m_bInTeamChat=2107;
inline constexpr uint32_t m_bInPartyChat=2108;
inline constexpr uint32_t m_bLaneSwapLocked=2109;
inline constexpr uint32_t m_vecLaneSwapRequests=2112;
inline constexpr uint32_t m_vecLaneSwapRejects=2136;
inline constexpr uint32_t m_vecMutedPlayers=2160;
inline constexpr uint32_t m_bCommsRestricted=2184;
inline constexpr uint32_t m_bPriorCommsAbuse=2185;
inline constexpr uint32_t m_bIsNewPlayer=2186;
inline constexpr uint32_t m_unEconAccountID=2188;
inline constexpr uint32_t m_hHeroPawn=2240;
inline constexpr uint32_t m_PlayerDataGlobal=2312;
inline constexpr uint32_t m_nDeathReplayAvailable=3152;
inline constexpr uint32_t m_unLobbyPlayerSlot=3153;
inline constexpr uint32_t m_bHasCheckedFriendName=3154;
inline constexpr uint32_t m_sFriendName=3160;
}
namespace PlayerDataGlobal_t{
inline constexpr uint32_t size=840;
inline constexpr uint32_t m_iLevel=8;
inline constexpr uint32_t m_iMaxAmmo=12;
inline constexpr uint32_t m_iHealthMax=16;
inline constexpr uint32_t m_flHealthRegen=20;
inline constexpr uint32_t m_flRespawnTime=24;
inline constexpr uint32_t m_nHeroID=28;
inline constexpr uint32_t m_nPreGameHeroID=32;
inline constexpr uint32_t m_unHeroBadgeXP=36;
inline constexpr uint32_t m_iGold=40;
inline constexpr uint32_t m_iGoldNetWorth=44;
inline constexpr uint32_t m_iAPNetWorth=48;
inline constexpr uint32_t m_iCreepGold=52;
inline constexpr uint32_t m_iCreepGoldSoloBonus=56;
inline constexpr uint32_t m_iCreepGoldKill=60;
inline constexpr uint32_t m_iCreepGoldAirOrb=64;
inline constexpr uint32_t m_iCreepGoldGroundOrb=68;
inline constexpr uint32_t m_iCreepGoldDeny=72;
inline constexpr uint32_t m_iCreepGoldNeutral=76;
inline constexpr uint32_t m_iFarmBaseline=80;
inline constexpr uint32_t m_iHealth=84;
inline constexpr uint32_t m_iPlayerKills=88;
inline constexpr uint32_t m_iNPCKills=92;
inline constexpr uint32_t m_iPlayerAssists=96;
inline constexpr uint32_t m_iDeaths=100;
inline constexpr uint32_t m_iDenies=104;
inline constexpr uint32_t m_iLastHits=108;
inline constexpr uint32_t m_iKillStreak=112;
inline constexpr uint32_t m_bAlive=116;
inline constexpr uint32_t m_nHeroDraftPosition=120;
inline constexpr uint32_t m_bUltimateTrained=124;
inline constexpr uint32_t m_flUltimateCooldownStart=128;
inline constexpr uint32_t m_flUltimateCooldownEnd=132;
inline constexpr uint32_t m_flUltimateStoppedCooldownRemaining=136;
inline constexpr uint32_t m_bHasRejuvenator=140;
inline constexpr uint32_t m_bHasRebirth=141;
inline constexpr uint32_t m_bFlaggedAsCheater=142;
inline constexpr uint32_t m_bAbandon=143;
inline constexpr uint32_t m_iHeroDamage=144;
inline constexpr uint32_t m_iHeroHealing=148;
inline constexpr uint32_t m_iSelfHealing=152;
inline constexpr uint32_t m_iObjectiveDamage=156;
inline constexpr uint32_t m_vecUpgrades=160;
inline constexpr uint32_t m_vecBonusCounterAbilities=184;
inline constexpr uint32_t m_vecBonusCounterValues=208;
inline constexpr uint32_t m_vecBonusCounterModifiers=232;
inline constexpr uint32_t m_vecModifierBonusCounterValues=256;
inline constexpr uint32_t m_tHeldItem=280;
inline constexpr uint32_t m_vecImbuements=288;
inline constexpr uint32_t m_vecDynamicAbilityValues=392;
inline constexpr uint32_t m_vecStatViewerModifierValues=496;
inline constexpr uint32_t m_vecStolenAbilities=600;
inline constexpr uint32_t m_vecAbilityUpgradeState=704;
inline constexpr uint32_t m_strIconHeroCardOverride=808;
inline constexpr uint32_t m_strIconHeroCardCriticalOverride=816;
inline constexpr uint32_t m_strIconHeroCardGloatOverride=824;
inline constexpr uint32_t m_unPackedRank=832;
}
namespace C_BasePlayerPawn{
inline constexpr uint32_t size=4192;
inline constexpr uint32_t m_pWeaponServices=3712;
inline constexpr uint32_t m_pItemServices=3720;
inline constexpr uint32_t m_pAutoaimServices=3728;
inline constexpr uint32_t m_pObserverServices=3736;
inline constexpr uint32_t m_pWaterServices=3744;
inline constexpr uint32_t m_pUseServices=3752;
inline constexpr uint32_t m_pFlashlightServices=3760;
inline constexpr uint32_t m_pCameraServices=3768;
inline constexpr uint32_t m_pMovementServices=3776;
inline constexpr uint32_t m_ServerViewAngleChanges=3792;
inline constexpr uint32_t v_angle=3896;
inline constexpr uint32_t v_anglePrevious=3908;
inline constexpr uint32_t m_iHideHUD=3920;
inline constexpr uint32_t m_skybox3d=3928;
inline constexpr uint32_t m_flDeathTime=4072;
inline constexpr uint32_t m_vecPredictionError=4080;
inline constexpr uint32_t m_flPredictionErrorTime=4092;
inline constexpr uint32_t m_vecLastCameraSetupLocalOrigin=4128;
inline constexpr uint32_t m_flLastCameraSetupTime=4140;
inline constexpr uint32_t m_flFOVSensitivityAdjust=4144;
inline constexpr uint32_t m_flMouseSensitivity=4148;
inline constexpr uint32_t m_vOldOrigin=4152;
inline constexpr uint32_t m_flOldSimulationTime=4164;
inline constexpr uint32_t m_nLastExecutedCommandNumber=4168;
inline constexpr uint32_t m_nLastExecutedCommandTick=4172;
inline constexpr uint32_t m_hController=4176;
inline constexpr uint32_t m_hDefaultController=4180;
inline constexpr uint32_t m_bIsSwappingToPredictableController=4184;
}
namespace CPlayer_ObserverServices{
inline constexpr uint32_t size=96;
inline constexpr uint32_t m_iObserverMode=72;
inline constexpr uint32_t m_hObserverTarget=76;
inline constexpr uint32_t m_iObserverLastMode=80;
inline constexpr uint32_t m_bForcedObserverMode=84;
inline constexpr uint32_t m_flObserverChaseDistance=88;
inline constexpr uint32_t m_flObserverChaseDistanceCalcTime=92;
}
namespace C_CitadelPlayerPawn{
inline constexpr uint32_t size=6480;
inline constexpr uint32_t m_hAbilityRequiresDebounce=4336;
inline constexpr uint32_t m_angEyeAngles=4376;
inline constexpr uint32_t m_angClientCamera=4520;
inline constexpr uint32_t m_eZipLineLaneColor=4664;
inline constexpr uint32_t m_nMapDistrictLocation=4668;
inline constexpr uint32_t m_nLevel=4672;
inline constexpr uint32_t m_nCurrencies=4676;
inline constexpr uint32_t m_nSpentCurrencies=4700;
inline constexpr uint32_t m_nNumHeroChangesUsed=4724;
inline constexpr uint32_t m_flLastSpawnTime=4728;
inline constexpr uint32_t m_flRespawnTime=4732;
inline constexpr uint32_t m_bInRegenerationZone=4736;
inline constexpr uint32_t m_bInItemShopZone=4737;
inline constexpr uint32_t m_bInHideoutZone=4738;
inline constexpr uint32_t m_nLastEnteredTunnelID=4740;
inline constexpr uint32_t m_vecFullSellPriceItems=4744;
inline constexpr uint32_t m_vecFullSellPriceAbilityUpgrades=4768;
inline constexpr uint32_t m_vecQuickbuyQueue=4792;
inline constexpr uint32_t m_vecQuickbuySellQueue=4816;
inline constexpr uint32_t m_unQuickbuyAutoPurchaseRequest=4840;
inline constexpr uint32_t m_bQuickbuyAutoPurchase=4844;
inline constexpr uint32_t m_bQuickbuyAutoQueueBuild=4845;
inline constexpr uint32_t m_bHasQuickbuyBeenUsed=4846;
inline constexpr uint32_t m_vecRestrictedToItems=4848;
inline constexpr uint32_t m_unHeroBuildID=4872;
inline constexpr uint32_t m_sHeroBuildSerialized=4880;
inline constexpr uint32_t m_hViewEntityForObserver=4888;
inline constexpr uint32_t m_bNetworkDisconnected=4892;
inline constexpr uint32_t m_bLearningAbility=4893;
inline constexpr uint32_t m_nFlashStartTick=4896;
inline constexpr uint32_t m_nFlashMaxStartTick=4900;
inline constexpr uint32_t m_nFlashFadeStartTick=4904;
inline constexpr uint32_t m_nFlashEndTick=4908;
inline constexpr uint32_t m_nFlashMaxAlpha=4912;
inline constexpr uint32_t m_nDeducedLane=4916;
inline constexpr uint32_t m_hEnemyPlayerPrimaryAimTarget=4920;
inline constexpr uint32_t m_iEnemyPlayerAimTargetBitVec=4928;
inline constexpr uint32_t m_arrPreventAbilityLearning=4936;
inline constexpr uint32_t m_ItemDraftRoundState=4944;
inline constexpr uint32_t m_nStreetBrawlCorruptionsAvailable=5080;
inline constexpr uint32_t m_sInCombat=5088;
inline constexpr uint32_t m_sPlayerDamageTaken=5112;
inline constexpr uint32_t m_sPlayerDamageDealt=5136;
inline constexpr uint32_t m_tLastRevealTime=5160;
inline constexpr uint32_t m_tLastPlayerRevealTime=5164;
inline constexpr uint32_t m_nSuccessiveDucks=5168;
inline constexpr uint32_t m_flLastDuckTime=5172;
inline constexpr uint32_t m_bDismissedReportCard=5176;
inline constexpr uint32_t m_flCurrentHealingAmount=5180;
inline constexpr uint32_t m_angLockedEyeAngles=5184;
inline constexpr uint32_t m_bIgnoringZoom=5196;
inline constexpr uint32_t m_CCitadelAbilityComponent=5200;
inline constexpr uint32_t m_CCitadelHeroComponent=5680;
inline constexpr uint32_t m_vLastVelocity=5832;
inline constexpr uint32_t m_flRichPresenceUpdateInterval=5900;
inline constexpr uint32_t m_nQueuedAbility=6128;
inline constexpr uint32_t m_QueuedAbilityEndTime=6136;
inline constexpr uint32_t m_flPredTimeSlowedStart=6144;
inline constexpr uint32_t m_flPredTimeSlowedEnd=6148;
inline constexpr uint32_t m_flPredSlowSpeed=6152;
inline constexpr uint32_t m_flTimeSlowedStart=6156;
inline constexpr uint32_t m_flTimeSlowedEnd=6172;
inline constexpr uint32_t m_flSlowSpeed=6188;
inline constexpr uint32_t m_flForceInCombatAnimsUntilTime=6204;
inline constexpr uint32_t m_iCurSlowSlot=6208;
inline constexpr uint32_t m_bLocoLeanTriggeredForDirection=6212;
inline constexpr uint32_t m_bLocoRunToStopCanTrigger=6213;
inline constexpr uint32_t m_flCrouchFraction=6216;
inline constexpr uint32_t m_flCrouchSpeed=6220;
inline constexpr uint32_t m_fidgetTime=6224;
inline constexpr uint32_t m_vShootTestOffsetStanding=6228;
inline constexpr uint32_t m_vShootTestOffsetCrouching=6240;
inline constexpr uint32_t m_leanStartTime=6252;
inline constexpr uint32_t m_fAudioEnclosure=6396;
inline constexpr uint32_t m_bAudioHasSkyExposure=6400;
inline constexpr uint32_t m_vecEquippedItemIDs=6448;
}
namespace CCitadelAbilityComponent{
inline constexpr uint32_t size=480;
inline constexpr uint32_t m_vecAbilities=104;
inline constexpr uint32_t m_arPendingAsyncAbilityReservationSlots=152;
inline constexpr uint32_t m_arPendingAsyncAbilityReservationAbilityIDs=176;
inline constexpr uint32_t m_hSelectedAbility=200;
inline constexpr uint32_t m_hChannellingAbility=204;
inline constexpr uint32_t m_hCastDelayingAbility=208;
inline constexpr uint32_t m_hPreviouslySelectedAbility=212;
inline constexpr uint32_t m_bPreviousAbilityQueued=216;
inline constexpr uint32_t m_flTimeScale=220;
inline constexpr uint32_t m_flParticleTimeScale=224;
inline constexpr uint32_t m_bInInterruptState=228;
inline constexpr uint32_t m_ResourceStamina=232;
inline constexpr uint32_t m_ResourceAbility=296;
inline constexpr uint32_t m_vecConsumedComponents=360;
inline constexpr uint32_t m_bThinkableAbilitiesDirty=464;
inline constexpr uint32_t m_nExecuteAbilityMask=468;
inline constexpr uint32_t m_bSelectedEffectsStarted=476;
}
namespace C_CitadelBaseAbility{
inline constexpr uint32_t size=5856;
inline constexpr uint32_t m_vecIntrinsicModifiers=1744;
inline constexpr uint32_t m_pCastDelayAutoModifier=1768;
inline constexpr uint32_t m_pChannelAutoModifier=1792;
inline constexpr uint32_t m_bIsCoolingDownInternal=1820;
inline constexpr uint32_t m_flCancelMashProtectionEndTime=1824;
inline constexpr uint32_t m_flCancelLockoutEndTime=1828;
inline constexpr uint32_t m_bChanneling=1864;
inline constexpr uint32_t m_bInCastDelay=1865;
inline constexpr uint32_t m_bShouldBeExecuted=1866;
inline constexpr uint32_t m_bCanBeUpgraded=1867;
inline constexpr uint32_t m_eStolenInSlot=1872;
inline constexpr uint32_t m_nUpgradeInfo=1888;
inline constexpr uint32_t m_bToggleState=1892;
inline constexpr uint32_t m_flCooldownStart=1896;
inline constexpr uint32_t m_flCooldownEnd=1900;
inline constexpr uint32_t m_flStoppedCooldownRemaining=1904;
inline constexpr uint32_t m_flCastCompletedTime=1908;
inline constexpr uint32_t m_flChannelStartTime=1912;
inline constexpr uint32_t m_flCastDelayStartTime=1916;
inline constexpr uint32_t m_flCastDelaySpeedScale=1920;
inline constexpr uint32_t m_eAbilitySlot=1924;
inline constexpr uint32_t m_flPostCastDelayEndTime=1928;
inline constexpr uint32_t m_iRemainingCharges=1932;
inline constexpr uint32_t m_flChargeRechargeStart=1936;
inline constexpr uint32_t m_flChargeRechargeEnd=1940;
inline constexpr uint32_t m_flMovementControlActiveTime=1944;
inline constexpr uint32_t m_flSelectedChangedTime=1948;
inline constexpr uint32_t m_flAltCastHoldStartTime=1952;
inline constexpr uint32_t m_flAltCastDoubleTapStartTime=1956;
inline constexpr uint32_t m_bCanBeImbued=1960;
inline constexpr uint32_t m_vecImbuedAbilities=1968;
inline constexpr uint32_t m_bSelectionModeIsAltMode=1992;
inline constexpr uint32_t m_bPredErrorCheckChanneling=1993;
inline constexpr uint32_t m_bPredErrorCheckCasting=1994;
inline constexpr uint32_t m_flPredErrorCheckCastCompleteTime=1996;
inline constexpr uint32_t m_bPredErrorCheckIsSelected=2000;
inline constexpr uint32_t m_enActiveReasonBits=5196;
inline constexpr uint32_t m_flActiveWindowDeadlines=5200;
inline constexpr uint32_t m_flNextMeepMopTime=5796;
}
namespace CModifierProperty{
inline constexpr uint32_t size=16256;
inline constexpr uint32_t __m_pChainEntity=16;
inline constexpr uint32_t m_hOwner=56;
inline constexpr uint32_t m_vecModifiers=64;
inline constexpr uint32_t m_bPredictedOwner=16017;
inline constexpr uint32_t m_bAllowModifiersOnDeadEntities=16018;
inline constexpr uint32_t m_iLockRefCount=16019;
inline constexpr uint32_t m_hHandle=16020;
inline constexpr uint32_t m_nBroadcastEventListenerMask=16024;
inline constexpr uint32_t m_nCachedHighestParticleIndex=16028;
inline constexpr uint32_t m_pNotifyOwnerEvents=16032;
inline constexpr uint32_t m_nDisabledGroups=16040;
inline constexpr uint32_t m_bvEnabledStateMask=16044;
inline constexpr uint32_t m_bvDisabledStateMask=16092;
inline constexpr uint32_t m_bvEnabledPredictedStateMask=16140;
inline constexpr uint32_t m_bParentWantsModifierStateChangeCallback=16200;
}
namespace CCitadelHeroComponent{
inline constexpr uint32_t size=64;
inline constexpr uint32_t m_spawnedHero=24;
inline constexpr uint32_t m_loadingHero=40;
inline constexpr uint32_t m_nNoSpawnHeroID=56;
}
namespace CitadelHeroSpawnData_t{
inline constexpr uint32_t size=16;
inline constexpr uint32_t m_nHeroID=8;
inline constexpr uint32_t m_unHeroBadgeXP=12;
}
namespace C_CitadelGameRules{
inline constexpr uint32_t size=41200;
inline constexpr uint32_t m_bFreezePeriod=88;
inline constexpr uint32_t m_fLevelStartTime=92;
inline constexpr uint32_t m_flGameStartTime=96;
inline constexpr uint32_t m_flGameStateStartTime=100;
inline constexpr uint32_t m_flGameStateEndTime=104;
inline constexpr uint32_t m_flRoundStartTime=108;
inline constexpr uint32_t m_flPlayOfTheGameStateEndTime=112;
inline constexpr uint32_t m_eGameState=116;
inline constexpr uint32_t m_hTowerAmber=120;
inline constexpr uint32_t m_hTowerSapphire=124;
inline constexpr uint32_t m_bEnemyInAmberBase=128;
inline constexpr uint32_t m_bEnemyInSapphireBase=129;
inline constexpr uint32_t m_bEnemyPlayersInAmberBase=130;
inline constexpr uint32_t m_bEnemyPlayersInSapphireBase=131;
inline constexpr uint32_t m_vMinimapMins=132;
inline constexpr uint32_t m_vMinimapMaxs=144;
inline constexpr uint32_t m_bMatchSafeToAbandon=156;
inline constexpr uint32_t m_bMatchNotScored=157;
inline constexpr uint32_t m_tAbandonTriggerEarlyTime=160;
inline constexpr uint32_t m_bAbandonTriggerSapphire=164;
inline constexpr uint32_t m_bAbandonTriggerAmber=165;
inline constexpr uint32_t m_eMatchMode=168;
inline constexpr uint32_t m_eGameMode=172;
inline constexpr uint32_t m_unSpectatorCount=176;
inline constexpr uint32_t m_unExpectedPlayerCount=180;
inline constexpr uint32_t m_nHideoutOwner=184;
inline constexpr uint32_t m_hTrooperMinimap=188;
inline constexpr uint32_t m_iWinningTeam=192;
inline constexpr uint32_t m_vecBannedHeroes=200;
inline constexpr uint32_t m_nCorruptedPenaltySeed=224;
inline constexpr uint32_t m_nNumCorruptedItemsLimit=228;
inline constexpr uint32_t m_vecTeamKothStates=232;
inline constexpr uint32_t m_nKothScoringTeam=496;
inline constexpr uint32_t m_timeKothScoring=500;
inline constexpr uint32_t m_timeKothCashInStarted=504;
inline constexpr uint32_t m_timeKothGiveUp=508;
inline constexpr uint32_t m_nAmberGold=512;
inline constexpr uint32_t m_nSapphireGold=516;
inline constexpr uint32_t m_vKothCashInCurrentLocation=520;
inline constexpr uint32_t m_hCurrentHeroDrafterRebels=532;
inline constexpr uint32_t m_hCurrentHeroDrafterCombine=536;
inline constexpr uint32_t m_bDontUploadStats=540;
inline constexpr uint32_t m_iMidbossKillCount=544;
inline constexpr uint32_t m_iAmberRejuvCount=548;
inline constexpr uint32_t m_iSapphireRejuvCount=552;
inline constexpr uint32_t m_tNextMidBossSpawnTime=556;
inline constexpr uint32_t m_vecNeutralCampTimerOrigins=560;
inline constexpr uint32_t m_vecNeutralCampNextSpawnTimes=584;
inline constexpr uint32_t m_vecNeutralCampTimerIsMidBoss=608;
inline constexpr uint32_t m_bServerPaused=40912;
inline constexpr uint32_t m_iPauseTeam=40916;
inline constexpr uint32_t m_nMatchClockUpdateTick=40920;
inline constexpr uint32_t m_flMatchClockAtLastUpdate=40924;
inline constexpr uint32_t m_flPauseTime=40928;
inline constexpr uint32_t m_pausingPlayerId=40936;
inline constexpr uint32_t m_unpausingPlayerId=40940;
inline constexpr uint32_t m_fPauseRawTime=40944;
inline constexpr uint32_t m_fPauseCurTime=40948;
inline constexpr uint32_t m_fUnpauseRawTime=40952;
inline constexpr uint32_t m_fUnpauseCurTime=40956;
inline constexpr uint32_t m_nLastPreGameCount=41040;
inline constexpr uint32_t m_eGGTeam=41044;
inline constexpr uint32_t m_flGGEndsAtTime=41048;
inline constexpr uint32_t m_bGGMarkAsNotScored=41052;
inline constexpr uint32_t m_unMatchID=41056;
inline constexpr uint32_t m_sGameplayExperiment=41064;
inline constexpr uint32_t m_ExperimentTokenHashCode=41072;
inline constexpr uint32_t m_nPlayerDeathEventID=41076;
inline constexpr uint32_t m_nReplayChangedEvent=41080;
inline constexpr uint32_t m_nGameOverEvent=41084;
inline constexpr uint32_t m_flHeroDiedTime=41120;
inline constexpr uint32_t m_pPlayOfTheGame=41128;
inline constexpr uint32_t m_tStreetBrawl=41144;
}
namespace C_CitadelTeam{
inline constexpr uint32_t size=1848;
inline constexpr uint32_t m_flBaseObjectiveHealth=1704;
inline constexpr uint32_t m_vecBaseLocationX=1708;
inline constexpr uint32_t m_vecBaseLocationY=1712;
inline constexpr uint32_t m_bHasValidBaseLocation=1716;
inline constexpr uint32_t m_nBossesAlive=1720;
inline constexpr uint32_t m_nBossesMax=1724;
inline constexpr uint32_t m_nFlexSlotsUnlocked=1728;
inline constexpr uint32_t m_nBaseGuardianLanesCleared=1732;
inline constexpr uint32_t m_vecFOWEntities=1736;
inline constexpr uint32_t m_nStreetBrawlScore=1840;
inline constexpr uint32_t m_nStreetBrawlScoreLastRound=1844;
}
namespace STeamFOWEntity{
inline constexpr uint32_t size=96;
inline constexpr uint32_t m_nEntIndex=48;
inline constexpr uint32_t m_nTeam=52;
inline constexpr uint32_t m_eClass=56;
inline constexpr uint32_t m_iLane=60;
inline constexpr uint32_t m_eHeight=64;
inline constexpr uint32_t m_bVisibleOnMap=65;
inline constexpr uint32_t m_bBackdoorProtectionActive=66;
inline constexpr uint32_t m_nTickHidden=68;
inline constexpr uint32_t m_strCSSClass=72;
inline constexpr uint32_t m_nOwnerOnlyEntIndex=80;
inline constexpr uint32_t m_nHealthPercent=84;
inline constexpr uint32_t m_nPositionX=85;
inline constexpr uint32_t m_nPositionY=86;
}
namespace CCitadelBaseLockonAbility{
inline constexpr uint32_t size=7032;
inline constexpr uint32_t m_vecLockonTargets=6912;
inline constexpr uint32_t m_LockOnStartTime=7016;
inline constexpr uint32_t m_nTargetingLightEffect=7024;
}
namespace LockonTarget_t{
inline constexpr uint32_t size=104;
inline constexpr uint32_t m_flGainRate=48;
inline constexpr uint32_t m_flDrainRate=52;
inline constexpr uint32_t m_flMaxValue=56;
inline constexpr uint32_t m_nPrevFullStacks=60;
inline constexpr uint32_t m_flLatchedValue=64;
inline constexpr uint32_t m_flLatchedTime=68;
inline constexpr uint32_t m_eLockonState=72;
inline constexpr uint32_t m_hTarget=76;
}
namespace CCitadel_Ability_Lash_Ultimate{
inline constexpr uint32_t size=8112;
inline constexpr uint32_t m_EGrappleState=7034;
inline constexpr uint32_t m_flStateEnterTime=7036;
inline constexpr uint32_t m_flNextStateTime=7040;
inline constexpr uint32_t m_flBoostEndTime=7044;
}
namespace CCitadel_Ability_HoldMelee{
inline constexpr uint32_t size=6360;
inline constexpr uint32_t m_flStateStartTime=6224;
inline constexpr uint32_t m_flDashStartTime=6228;
inline constexpr uint32_t m_eCurrentAttackState=6232;
inline constexpr uint32_t m_eCurrentAttackType=6236;
inline constexpr uint32_t m_vAirDashDir=6240;
inline constexpr uint32_t m_bAttackStartedWhileSliding=6252;
inline constexpr uint32_t m_flLightChainEndTime=6256;
inline constexpr uint32_t m_nLightChainCount=6260;
inline constexpr uint32_t m_bCreatedChargeEffects=6264;
inline constexpr uint32_t m_angForced=6268;
inline constexpr uint32_t m_vGoalDir=6280;
}
namespace CCitadel_Ability_UltCombo{
inline constexpr uint32_t size=6400;
inline constexpr uint32_t m_flLastAttackTime=5856;
inline constexpr uint32_t m_nAttackNum=5860;
inline constexpr uint32_t m_iBonusHealth=6392;
inline constexpr uint32_t m_hTarget=6396;
}
namespace CCitadel_Ability_Hornet_Snipe{
inline constexpr uint32_t size=10160;
inline constexpr uint32_t m_flScopeStartTime=8172;
}
namespace C_CitadelProjectile{
inline constexpr uint32_t size=3304;
inline constexpr uint32_t m_flMaxDistance=3056;
inline constexpr uint32_t m_nCachedExcludeFlags=3064;
inline constexpr uint32_t m_bInPortalEnvironment=3072;
inline constexpr uint32_t m_bHandlingPortalResult=3073;
inline constexpr uint32_t m_flArmingTime=3076;
inline constexpr uint32_t m_flChargeAmount=3080;
inline constexpr uint32_t m_bCollideWithThrower=3084;
inline constexpr uint32_t m_bNewCollideWithThrower=3085;
inline constexpr uint32_t m_flTickSoundInterval=3096;
inline constexpr uint32_t m_nNumDetonations=3104;
inline constexpr uint32_t m_nDetonationsLeft=3108;
inline constexpr uint32_t m_vInitialVelocity=3112;
inline constexpr uint32_t m_vInitialPosition=3124;
inline constexpr uint32_t m_abilityID=3136;
inline constexpr uint32_t m_hParticleDef=3144;
inline constexpr uint32_t m_vecSpawnPosition=3152;
inline constexpr uint32_t m_flProjectileSpeed=3164;
inline constexpr uint32_t m_flMaxLifetime=3168;
inline constexpr uint32_t m_flParticleRadius=3176;
inline constexpr uint32_t m_flPreviousTimeScale=3296;
}
namespace CCitadel_Ability_Bull_Leap{
inline constexpr uint32_t size=7264;
inline constexpr uint32_t m_bBraceParamTriggered=5856;
inline constexpr uint32_t m_flBoostYaw=5860;
inline constexpr uint32_t m_vecCrashPosition=5864;
inline constexpr uint32_t m_vecCrashDirection=5876;
inline constexpr uint32_t m_eLeapState=5888;
inline constexpr uint32_t m_flStateEnterTime=5892;
inline constexpr uint32_t m_flNextStateTime=5896;
inline constexpr uint32_t m_flBoostEndTime=5920;
inline constexpr uint32_t m_vPrevPos=7176;
inline constexpr uint32_t m_vecDraggedEntities=7192;
inline constexpr uint32_t m_vecLastVel=7228;
inline constexpr uint32_t m_vecCrashDownLastPos=7240;
inline constexpr uint32_t m_bInputBufferCrash=7252;
}
namespace CCitadel_Ability_LashDownStrike{
inline constexpr uint32_t size=8040;
inline constexpr uint32_t m_ImpactTime=6384;
inline constexpr uint32_t m_vDamagePos=6388;
inline constexpr uint32_t m_PreviewEffect=6400;
inline constexpr uint32_t m_ActiveEffect=6404;
inline constexpr uint32_t m_bIsCrashingDown=8016;
inline constexpr uint32_t m_vStrikeVel=8020;
inline constexpr uint32_t m_flInitialYaw=8032;
inline constexpr uint32_t m_flStartHeight=8036;
}
namespace CCitadel_Ability_Bull_Charge{
inline constexpr uint32_t size=8192;
inline constexpr uint32_t m_anglesCharging=8144;
inline constexpr uint32_t m_flChargeStartTime=8156;
inline constexpr uint32_t m_flFastChargeStartTime=8160;
inline constexpr uint32_t m_flFastChargeEndTime=8164;
inline constexpr uint32_t m_bHitSomethingStunnable=8168;
inline constexpr uint32_t m_bFirstTick=8172;
inline constexpr uint32_t m_vGoalDir=8176;
}
struct Class{uint32_t vtable;const char* name;int kind;};
inline constexpr Class classes[]={
{42732616,"C_PropDoorRotating",9},
{43647632,"C_Citadel_FissureWall",9},
{46281032,"C_BaseDoor",9},
{46327528,"C_BasePropDoor",9},

{46235936u,"C_World",8},

{40236696,"CCitadel_Ability_Climb_Rope",4},
{40240952,"CCitadel_Ability_Jump",4},
{40244528,"CCitadel_Ability_Dash",4},
{40248192,"CCitadel_Ability_Slide",4},
{40251944,"CCitadel_Ability_PrimaryWeapon",4},
{40255592,"CCitadel_Ability_PrimaryWeapon_Empty",4},
{40259240,"CCitadel_Ability_PrimaryWeapon_ScalingAltFire",4},
{40263496,"CCitadel_Ability_Mantle",4},
{40267616,"CCitadel_Ability_Melee_Base",4},
{40271312,"CCitadel_Ability_HoldMelee",4},
{40275544,"CCitadel_Ability_MeleeParry",4},
{40280848,"CCitadel_Ability_Sprint",4},
{40284272,"CCitadel_Ability_Teleport",4},
{40675024,"CCitadel_Ability_ZipLine",4},
{40678792,"CCitadel_Ability_TrooperZipLine",4},
{40682376,"CCitadel_Ability_ZipLine_Boost",4},
{40687952,"CCitadel_Ability_Weapon_BossTier2",4},
{40691528,"CCitadel_Ability_Tier2Boss_RocketBarrage",4},
{40695104,"CCitadel_Ability_Tier2Boss_LaserBeam",4},
{40698504,"CCitadel_Ability_Tier2Boss_Stomp",4},
{40702192,"CCitadel_Ability_Tier2Boss_AoEWave",4},
{40705880,"CCitadel_Ability_Weapon_BossTier3",4},
{40709624,"CCitadel_Ability_Tier3Boss_LaserBeam",4},
{40713256,"CCitadel_Ability_Tier3Boss_AoEWave",4},
{40717000,"CCitadel_Ability_Tier3Boss_DropBombs",4},
{40720576,"CCitadel_Ability_Tier3Boss_RocketBarrage",4},
{40723976,"CCitadel_Ability_TriggerTower",4},
{40727376,"CCitadel_Ability_TrooperGrenade",4},
{40730816,"CCitadel_Ability_TrooperBossGrenade",4},
{40734568,"CCitadel_Ability_MedicHeal",4},
{40737968,"CCitadel_Ability_SuperNeutralShield",4},
{40741368,"CCitadel_Ability_SuperNeutralIncendiary",4},
{40744912,"CCitadel_Ability_SuperNeutralCharge",4},
{40748720,"CCitadel_ArmorUpgrade_DoubleJump",4},
{40752376,"CCitadel_Upgrade_WeaponPowerForHealth",4},
{40755976,"CCitadel_Item_HealthRegenAura",4},
{40759576,"CCitadel_Item_ComboBreaker",4},
{40763232,"CCitadel_Item_CheatDeath",4},
{40766888,"CCitadel_ArmorUpgrade_ActiveBulletShield",4},
{40770488,"CCitadel_ArmorUpgrade_Grit",4},
{40774088,"CCitadel_ArmorUpgrade_WeaponShielding",4},
{40777688,"CCitadel_ArmorUpgrade_SpiritBubble",4},
{40781104,"CCitadel_ArmorUpgrade_RegeneratingBulletShield",4},
{40784760,"CCitadel_ArmorUpgrade_AblativeCoat",4},
{40788416,"CCitadel_ArmorUpgrade_ReturnFire",4},
{40791832,"CCitadel_ArmorUpgrade_BulletArmorReductionAura",4},
{40795488,"CCitadel_ArmorUpgrade_MetalSkin",4},
{40798904,"CCitadel_Item_Camouflage",4},
{40802560,"CCitadel_ArmorUpgrade_Stimpak",4},
{40806216,"CCitadel_Item_GuardianWard",4},
{40809872,"CCitadel_Item_DivineBarrier",4},
{40813472,"CCitadel_ArmorUpgrade_PersonalRejuvenator",4},
{40817072,"CItem_RestorativeLocket",4},
{40820728,"CCitadel_ArmorUpgrade_Colossus",4},
{40824200,"CCitadel_ArmorUpgrade_Frenzy",4},
{40827616,"CCitadel_ArmorUpgrade_DamageRecycler",4},
{40831216,"CCitadel_ArmorUpgrade_DebuffReducer",4},
{40834632,"CCitadel_ArmorUpgrade_HighImpactArmor",4},
{40838232,"CCitadel_ArmorUpgrade_SlowImmunity",4},
{40841832,"CCitadel_ArmorUpgrade_RegenerativeArmor",4},
{40845432,"CCitadel_ArmorUpgrade_VexBarrier",4},
{40849032,"CCitadel_ArmorUpgrade_AutoCleanse",4},
{40852632,"CCitadel_ArmorUpgrade_SpellShield",4},
{40856232,"CCitadel_ArmorUpgrade_HealOnLevel",4},
{40860168,"CCitadel_Item_TrophyCollector",4},
{40863824,"CCitadel_ArmorUpgrade_Shrink_Ray",4},
{40867960,"CCitadel_CosmeticAbility_Snowball",4},
{40871408,"CCitadel_CosmeticAbility_VotingPoster",4},
{40874984,"CCitadel_CosmeticAbility_Emote",4},
{40878536,"C_Citadel_BreakableProp",7},
{40881120,"CCitadel_SeasonalAbility_EatHalloweenCandy",4},
{40884576,"C_Citadel_BreakableProp_PunchableTroll",7},
{40887008,"C_Citadel_Pickup",7},
{40889384,"CCitadel_Pickup_HalloweenCandy",7},
{40892016,"CCitadel_Item_TrackingProjectileApplyModifier",4},
{40895432,"CCitadel_Item_RejuvTrackingProjectile",4},
{40899032,"CCitadel_Item_SelfBuffModifier",4},
{40902688,"CItem_WitheringWhip",4},
{40906104,"CItem_GreaterWitheringWhip",4},
{40909704,"CCitadel_Upgrade_AmmoScavenger",4},
{40913360,"CCitadel_Item_Bubble",4},
{40917240,"CCitadel_Item_PrismBlast",4},
{40920840,"CCitadel_Item_Stasis_Bomb",4},
{40924496,"CCitadel_TechUpgrade_Infuser",4},
{40928264,"CCitadel_Item_RescueBeam",4},
{40931976,"CCitadel_Item_Refresher",4},
{40935576,"CCitadel_Item_PowerShard",4},
{40939192,"CCitadel_Item_AOE_Tech_Shield",4},
{40942848,"CCitadel_Item_SingleTargetStun",4},
{40946448,"CCitadel_Item_PhantomStrike",4},
{40950048,"CCitadel_Item_AOERoot",4},
{40953872,"CCitadel_Item_TechCleave",4},
{40957640,"CCitadel_Item_ShadowStep",4},
{40961240,"CCitadel_Item_TechDamagePulse",4},
{40964736,"CCitadel_ArmorUpgrade_AbilityLifeSteal",4},
{40968208,"CItemMysticReverb",4},
{40971904,"CCitadel_Item_Charge_Mastery",4},
{40975728,"CCitadel_Item_DivinersKevlar",4},
{40979440,"CCitadel_Item_Containment",4},
{40983040,"CCitadel_Item_SpiritSap",4},
{40986640,"CCitadel_Item_FocusLens",4},
{40990296,"CCitadel_Item_Disarm",4},
{40994008,"CCitadel_Item_ColdFront",4},
{40997664,"CCitadel_Item_ArcticBlast",4},
{41001880,"CItemSilenceGlyph",4},
{41005480,"CItemHauntingScream",4},
{41009080,"CItem_ResonantHealing",4},
{41012680,"CCitadel_Upgrade_MagicCarpet",4},
{41016376,"CCitadel_TechUpgrade_SuperAcolyteGloves",4},
{41020312,"CCitadel_TechUpgrade_CorpseExplosion",4},
{41023912,"CCitadel_Item_Mystic_Regeneration",4},
{41028056,"CCitadel_UtilityUpgrade_HealthNova",4},
{41033624,"CCitadel_Ability_Shield",4},
{41037224,"CCitadel_ArmorUpgrade_CloakingDeviceActive",4},
{41040696,"CCitadel_ArmorUpgrade_CloakingDevice",4},
{41044352,"CCitadel_UtilityUpgrade_RocketBoots",4},
{41047968,"CCitadel_UtilityUpgrade_RocketBooster",4},
{41051584,"CCitadel_Upgrade_AerialAssault",4},
{41055352,"CCitadel_UtilityUpgrade_AOESmokeBomb",4},
{41059064,"CCitadel_UtilityUpgrade_DebuffImmunity",4},
{41062664,"CCitadel_Item_ModDisruptor",4},
{41066264,"CCitadel_Item_BaseProjectileAOEModifier",4},
{41069864,"CCitadel_Item_ProjectileTest",4},
{41073464,"CCitadel_Item_ProjectileTest02",4},
{41077064,"CCitadel_Item_ProjectileTest04",4},
{41080664,"CCitadel_Item_ProjectileTest05",4},
{41084264,"CCitadel_Item_ProjectileTest06",4},
{41088032,"CCitadel_Item_NullificationAura",4},
{41091632,"CCitadel_Item_CelestialGuidance",4},
{41095456,"CCitadel_Item_Electric_Slippers",4},
{41099224,"CCitadel_Item_ShadowStrike",4},
{41102640,"CCitadel_Omnicharge_Pendant",4},
{41106056,"CCitadel_Item_GooseEgg",4},
{41109888,"CCitadel_WeaponUpgrade_CooldownOnMiss",4},
{41113976,"CCitadel_WeaponUpgrade_FireRateAura",4},
{41117968,"CCitadel_WeaponUpgrade_SiphonBullets",4},
{41121664,"CCitadel_WeaponUpgrade_Ricochet",4},
{41125136,"CCitadel_WeaponUpgrade_ApexCombat",4},
{41129016,"CCitadel_WeaponUpgrade_WeaponEater",4},
{41132616,"CCitadel_WeaponUpgrade_InstantReload",4},
{41136272,"CCitadel_WeaponUpgrade_FuryTrance",4},
{41139928,"CCitadel_WeaponUpgrade_SurgingPower",4},
{41143584,"CCitadel_Upgrade_OverdriveClip",4},
{41147352,"CItem_FleetfootBoots",4},
{41151176,"CCitadel_WeaponUpgrade_GlassCannon",4},
{41154888,"CCitadel_WeaponUpgrade_SpellslingerHeadshots",4},
{41158488,"CCitadel_WeaponUpgrade_HeadshotDamage",4},
{41162144,"CCitadel_WeaponUpgrade_BurstFire",4},
{41165744,"CCitadel_Item_DPS_Aura",4},
{41169400,"CCitadel_Item_WarpStone",4},
{41172928,"CCitadel_Item_Intensifying_Clip",4},
{41176752,"CCitadel_Item_ActiveReload",4},
{41180352,"CCitadel_WeaponUpgrade_InfiniteMagazine",4},
{41184008,"CCitadel_WeaponUpgrade_SplitShot",4},
{41187608,"CCitadel_WeaponUpgrade_BloodTribute",4},
{41191208,"CCitadel_WeaponUpgrade_CultistSacrifice",4},
{41194808,"CItemCapacitor",4},
{41198576,"CCitadel_WeaponUpgrade_ExpressShot",4},
{41202232,"CCitadel_WeaponUpgrade_RechargingBullets",4},
{41216624,"CCitadel_Modifier_StreetBrawlTrooper",5},
{41221840,"?$CModifierFactory@VCCitadel_Modifier_StreetBrawlTrooper@@$0PPPP@",5},
{41354272,"CCitadel_Modifier_TrooperDisabledInvulnerabilityFX",5},
{41357648,"?$CModifierFactory@VCCitadel_Modifier_TrooperDisabledInvulnerabilityFX@@$0PPPP@",5},
{41358144,"CCitadel_Modifier_TrooperDisabledInvulnerability",5},
{41361936,"?$CModifierFactory@VCCitadel_Modifier_TrooperDisabledInvulnerability@@$0PPPP@",5},
{41372312,"CCitadel_Modifier_Trooper_InEnemyBaseResist",5},
{41377864,"?$CModifierFactory@VCCitadel_Modifier_Trooper_InEnemyBaseResist@@$0PPPP@",5},
{41386600,"CCitadel_Modifier_Trooper_ShrineDownBuff",5},
{41391720,"?$CModifierFactory@VCCitadel_Modifier_Trooper_ShrineDownBuff@@$0PPPP@",5},
{41400240,"CCitadel_Modifier_NPC_OutOfCombat",5},
{41402360,"?$CModifierFactory@VCCitadel_Modifier_NPC_OutOfCombat@@$0PPPP@",5},
{41410208,"CCitadel_Modifier_NPC_InCombat",5},
{41413688,"?$CModifierFactory@VCCitadel_Modifier_NPC_InCombat@@$0PPPP@",5},
{41439504,"CCitadel_Ability_Empty",4},
{41446496,"CCitadel_Modifier_Trooper_InEnemyBaseResistVData",5},
{41446552,"CCitadel_Modifier_Trooper_ShrineDownBuffVData",5},
{41446608,"CCitadel_Modifier_NPC_OutOfCombatVData",5},
{41446664,"CCitadel_Modifier_NPC_InCombatVData",5},
{41463024,"CCitadel_Item",4},
{41466440,"CCitadel_Item_Empty",4},
{41469856,"CCitadel_CosmeticAbility",4},
{41475176,"C_Citadel_BreakableProp_Hideout",7},
{41484120,"C_Citadel_Pickup_Ability",7},
{41486496,"C_Citadel_Pickup_Item",7},
{41488872,"C_Citadel_Pickup_Health",7},
{41491248,"C_Citadel_Pickup_Gold",7},
{41493640,"C_Citadel_Pickup_Currency",7},
{41496032,"C_Citadel_Pickup_AssignedCurrencies",7},
{41498408,"C_Citadel_Pickup_NecroDeath",7},
{41500784,"C_Citadel_Pickup_Modifier",7},
{41519680,"C_CitadelBaseAbility",4},
{41526176,"C_CitadelPlayerBotNPCBrain",5},
{41530080,"C_CitadelPlayerPawn",1},
{41533208,"C_CitadelTeam",3},
{41676192,"CAI_BaseNPCVData",5},
{41676272,"CAI_CitadelNPCVData",5},
{41676328,"CNPC_TrooperNeutralVData",5},
{41676384,"CNPC_TrooperNeutralNodeMoverVData",5},
{41676440,"CNPC_NeutralSinnerSacrificeVData",5},
{41676496,"CNPC_NeutralSinnerSacrificeHideoutVData",5},
{41676552,"CNPC_MidBossVData",5},
{41719584,"CCitadelBaseDashCastAbility",4},
{41723712,"CCitadel_Item_Aura_Base",4},
{41727352,"CCitadelBaseLockonAbility",4},
{41731008,"CCitadelBaseTieredLockonAbility",4},
{41734912,"CCitadelBaseTriggerAbility",4},
{41782240,"CCitadelPreviewPlayerController",2},
{41911304,"C_CitadelItemPickup",7},
{41918552,"CCitadelItemPickupRejuv",7},
{41920968,"CCitadelItemPickupRejuvHeroTest",7},
{41925120,"CCitadelItemPickupIdol",7},
{41927712,"C_ItemCrate",7},
{41932448,"CCitadel_Ability_GoldenIdol",4},
{42019440,"CCitadelPlayerController",2},
{42100576,"CCitadelTrooperMinimap",5},
{42129440,"CCitadel_Modifier_KothTrooperBuff",5},
{42130528,"?$CModifierFactory@VCCitadel_Modifier_KothTrooperBuff@@$0PPPP@",5},
{42171880,"CCitadel_Modifier_KothTrooperBuffVData",5},
{42214096,"CNPC_Boss_Tier2VData",5},
{42214152,"CNPC_Boss_Tier3VData",5},
{42214264,"CNPC_Escort_VData",5},
{42214320,"C_NPC_Escort",5},
{42216888,"CCitadel_NPCAbility_Vanguard_AOEBuff_VData",5},
{42217064,"CCitadel_NPCAbility_Vanguard_AOEBuff",4},
{42220464,"CCitadel_NPCAbility_Shield_VData",5},
{42220640,"CCitadel_NPCAbility_Shield",4},
{42224136,"CNPC_SimpleAnimatingAIVData",5},
{42224192,"CNPC_ShieldedSentryVData",5},
{42224248,"CAI_NPC_TrooperVData",5},
{42224304,"CNPC_TrooperBossVData",5},
{42224360,"CNPC_BarrackBossVData",5},
{42292128,"CCitadelBeamNPC",5},
{42292152,"C_AI_CitadelNPC",5},
{42294840,"C_NPC_Boss_Tier3Core",5},
{42297176,"C_NPC_Boss_Tier3",5},
{42299744,"C_NPC_BarrackBoss",5},
{42302376,"C_NPC_BaseDefenseSentry",5},
{42304736,"C_NPC_TrooperBoss",5},
{42307328,"C_NPC_Boss_Tier2",5},
{42310000,"C_NPC_Boss_Tier2_Sidelanes",5},
{42315312,"C_NPC_FlyingDrone",5},
{42317880,"C_NPC_CarpetBombDrone",5},
{42320472,"C_NPC_HornetDrone",5},
{42323064,"C_NPC_TrooperNeutral",5},
{42325648,"C_NPC_Neutral_SinnersSacrifice",5},
{42328232,"C_NPC_Neutral_SinnersSacrifice_Hideout",5},
{42330920,"C_NPC_TrooperNeutralNodeMover",5},
{42333504,"C_NPC_MidBoss",5},
{42336072,"C_NPC_Neutral_Flying_Pigeon",5},
{42338408,"C_NPC_Neutral_Hideout_Cat",5},
{42340744,"C_NPC_Neutral_Hideout_Rabbit",5},
{42347456,"C_NPC_Neutral_Bug",5},
{42350288,"C_NPC_PestilenceDrone",5},
{42355488,"C_NPC_SimpleAnimatingAI",5},
{42357824,"C_NPC_ShieldedSentry",5},
{42360248,"C_NPC_SurveillanceDrone",5},
{42362840,"C_NPC_Trooper",5},
{43008720,"CCitadel_Ability_PowerJump",4},
{43012296,"C_Citadel_Projectile_Archer_ChargedShot",6},
{43014408,"CCitadel_Ability_ChargedShot",4},
{43017984,"C_Citadel_Projectile_Guided_Arrow",6},
{43022664,"CCitadel_Ability_GuidedArrow",4},
{43026312,"C_CitadelProjectile_ImmobilizeTrap",6},
{43028568,"CCitadel_Ability_ImmobilizeTrap",4},
{43032264,"C_CitadelTrackedProjectile",6},
{43034552,"CCitadel_Ability_Artist_InkActive",4},
{43038128,"CCitadel_Ability_Artist_InkBrush",4},
{43044144,"CCitadel_Ability_Artist_BouncingPot",4},
{43047544,"CProjectile_Artist_BouncingPot",6},
{43049944,"CCitadel_Ability_Artist_HealingBarrier",4},
{43053520,"CCitadel_Ability_Artist_PrimaryWeapon",4},
{43057400,"CCitadel_Ability_Artist_DamageZone",4},
{43061032,"CCitadel_Ability_Artist_RollingProjectile",4},
{43064432,"C_Projectile_Artist_RollingProjectile",6},
{43066720,"CCitadel_Ability_Artist_ShieldProjectile",4},
{43070296,"CCitadel_Ability_Artist_ProjectileWeapon",4},
{43073944,"CCitadel_Projectile_ArtistGun",6},
{43076232,"CCitadel_Ability_Artist_TransformingProjectile",4},
{43079808,"CCitadel_Ability_Artist_StoneTurtle",4},
{43083384,"CCitadel_Ability_Artist_RideTiger",4},
{43087016,"CCitadel_Ability_Artist_MultiHit",4},
{43090592,"CCitadel_Ability_Artist_Cloudburst",4},
{43094224,"CCitadel_Ability_Artist_Pillar",4},
{43097856,"CCitadel_Ability_Artist_EmpowerAbilities",4},
{43101432,"CCitadel_Ability_Artist_RootingProjectile",4},
{43108136,"CCitadel_Ability_Artist_PullBrush",4},
{43111744,"CCitadel_Ability_Artist_HomingProjectile",4},
{43115320,"CCitadel_Ability_Artist_PrimaryWeapon2",4},
{43119144,"CCitadel_Ability_Artist_LionDance",4},
{43122656,"CCitadel_Pickup_ArtistDrawing",7},
{43125296,"CCitadel_Ability_Artist_Tether",4},
{43129072,"CCitadel_Ability_Artist_FlyingLion",4},
{43135088,"CCitadel_Ability_Artist_BambooGrove",4},
{43140312,"CItemExplosiveBarrel",6},
{43142424,"CCitadel_Ability_ExplosiveBarrel",4},
{43148656,"CCitadel_Ability_Bounce_Pad",4},
{43152232,"CCitadel_Ability_Gravity_Lasso",4},
{43155864,"CCitadel_Ability_Crackshot",4},
{43159664,"CCitadel_Projectile_BubblingBrew",6},
{43161952,"CCitadel_Ability_Baba_BubblingBrew",4},
{43165720,"CCitadel_Ability_Baba_BenchRun",4},
{43169344,"CCitadel_Ability_Baba_BenchMelee",4},
{43173032,"CCitadel_Ability_Baba_HexingBrew",4},
{43176608,"CCitadel_Ability_Baba_HexingBrew_Throw",4},
{43180352,"CCitadel_Ability_Baba_Ultimate2",4},
{43185024,"CCitadel_Ability_PrimaryWeapon_Bebop",4},
{43188856,"CCitadel_Ability_Hook",4},
{43192264,"C_Citadel_Projectile_Bebop_Hook",6},
{43194720,"CCitadel_Ability_StickyBomb",4},
{43198352,"CCitadel_Ability_Uppercut",4},
{43202104,"CCitadel_Ability_Bebop_LaserBeam",4},
{43205704,"CCitadel_Ability_Bebop_StickyBomb2",4},
{43209944,"CCitadel_Ability_Boho_Ability01",4},
{43213344,"CCitadel_Ability_Boho_Ability02",4},
{43216976,"CCitadel_Ability_Boho_DamageShare",4},
{43220552,"CCitadel_Ability_Boho_BouncyProjectile",4},
{43223952,"CProjectile_Boho_BouncyProjectile",6},
{43226240,"CCitadel_Ability_Boho_DoubleHit",4},
{43229880,"CCitadel_Ability_Boho_ChannelTether",4},
{43233464,"CCitadel_Ability_Boho_PrimaryWeapon",4},
{43237288,"CCitadel_Ability_Boho_SkipGrenade",4},
{43240688,"C_Projectile_Boho_SkipGrenade",6},
{43242976,"CCitadel_Ability_Boho_RideBroom",4},
{43246832,"CCitadel_Ability_Charged_Bomb",4},
{43250288,"CCitadel_Ability_Bomber_Ability02",4},
{43253688,"CCitadel_Ability_Bomber_Ability03",4},
{43257088,"CCitadel_Ability_Bomber_ULT",4},
{43260760,"CCitadel_Ability_Bookworm_DragonFire",4},
{43264160,"CProjectile_BookwormDragon_Projectile",6},
{43266616,"CCitadel_Ability_Bookworm_AOEMagic",4},
{43270248,"CCitadel_Ability_Bookworm_KnightBarrier",4},
{43273824,"CCitadel_Ability_Bookworm_KnightCharge",4},
{43277224,"C_Projectile_KnightCharge_Projectile",6},
{43279336,"CProjectile_KnightChargeLeading_Projectile",6},
{43281448,"CCitadel_Projectile_BookwormGun",6},
{43283736,"CCitadel_Ability_BookWorm_PrimaryWeapon",4},
{43288704,"CCitadel_Ability_Bull_Charge",4},
{43292656,"CCitadel_Ability_Bull_Leap",4},
{43296328,"CCitadel_Ability_Bull_Heal",4},
{43299904,"CCitadel_Ability_PassiveBeefy",4},
{43303888,"CCitadel_Ability_Cadence_Anthem",4},
{43307576,"CCitadel_Ability_Cadence_Lullaby",4},
{43311264,"CCitadel_Ability_Cadence_SilenceContraptions",4},
{43317384,"CCitadel_Ability_Cadence_GrandFinale",4},
{43321016,"CCitadel_Ability_PrimaryWeapon_Cadence",4},
{43325008,"CCitadel_Ability_Cadence_Crescendo",4},
{43328440,"CAI_NPC_ChessPieceVData",5},
{43328496,"CNPC_ChessPiece",5},
{43331408,"CCitadel_Ability_Chessmaster_MoveChessPiece",4},
{43335048,"CCitadel_Ability_Chessmaster_Queen",4},
{43338680,"CCitadel_Ability_Chessmaster_ThrowBishop",4},
{43342424,"CCitadel_Ability_Chessmaster_ForkPiece",4},
{43345824,"C_Projectile_ChessMaster_Fork_Projectile",6},
{43348168,"CCitadel_Ability_Chessmaster_KnightBuff",4},
{43351800,"CCitadel_Ability_ChessMaster_PrimaryWeapon",4},
{43356336,"CCitadel_Ability_ChronoSwap",4},
{43360088,"CCitadel_Ability_Chrono_TimeWall",4},
{43363664,"CCitadel_Ability_Chrono_KineticCarbine",4},
{43367352,"CCitadel_Ability_Chrono_PulseGrenade",4},
{43371304,"CCitadel_Projectile_DeadpackMissile",6},
{43373416,"CCitadel_Ability_Deadpack_ChargeBlast",4},
{43377104,"CCitadel_Ability_Deadpack_Rocketeer",4},
{43380776,"CCitadel_Ability_Deadpack_PrimaryWeapon_Charge",4},
{43384696,"CCitadel_Ability_Deadpack_Ultimate",4},
{43388328,"CCitadel_Ability_Deadpack_Flamethrower",4},
{43392528,"CCitadel_Ability_UltCombo",4},
{43396216,"CCitadel_Ability_Intimidate",4},
{43399904,"CCitadel_Ability_Burrow",4},
{43403480,"CCitadel_Ability_ThrowSand",4},
{43407112,"CCitadel_Ability_Digger_EnterTunnel",4},
{43410888,"CCitadel_Ability_Doorman_Doorway",4},
{43414712,"CCitadel_Ability_AbilityName",4},
{43418288,"CCitadel_Ability_Doorman_Cart",4},
{43421744,"C_Projectile_Doorman_Cart_Projectile",6},
{43423856,"CDoormanBombProjectile",6},
{43426200,"CCitadel_Ability_Doorman_Bomb",4},
{43430032,"CCitadel_Ability_Doorman_Hotel",4},
{43433952,"CCitadel_Ability_Drifter_PrimaryWeapon",4},
{43437776,"CAbility_Drifter_BloodBlast",4},
{43441352,"CAbility_Drifter_ShadowMark",4},
{43444928,"CAbility_Drifter_StalkersMark_Teleport",4},
{43448728,"CAbility_Drifter_Darkness",4},
{43452360,"CCitadel_Ability_Drifter_Hunger",4},
{43456640,"CCitadelAbilityDruidLeechSeed",4},
{43460216,"CCitadelAbilityDruidHelicopterSeeds",4},
{43473328,"CCitadelAbilityDruidSprout",4},
{43476904,"CCitadelAbilityDruidBasePlant",4},
{43480480,"CCitadelAbilityDruidPlantBranchWall",4},
{43484056,"CCitadelAbilityDruidPlantInvisBush",4},
{43487632,"CCitadelAbilityDruidPlantHealingTree",4},
{43491208,"CCitadelAbilityDruidPlantSomething",4},
{43494624,"CCitadelAbilityDruidAbility04",4},
{43644088,"CCitadel_Ability_ShieldedSentry",4},
{43650232,"CCitadel_Ability_FissureWall",4},
{43654000,"CCitadel_Ability_RocketBarrage",4},
{43660360,"CCitadel_Ability_MobileResupply",4},
{43664544,"CCitadel_Ability_Familiar_Ability02",4},
{43668192,"CCitadel_Ability_Familiar_Ability01",4},
{43671856,"CCitadel_Ability_Familiar_Spotlight",4},
{43675432,"CCitadel_Ability_Familiar_PrimaryWeapon",4},
{43679256,"CCitadel_Ability_Familiar_AltWeapon",4},
{43683136,"CCitadel_Ability_Familiar_Attach",4},
{43686712,"CCitadel_Ability_Familiar_Attach_Trigger",4},
{43690112,"CProjectile_Familiar_MovingToAttach",6},
{43692512,"CCitadel_Ability_Familiar_HealHost",4},
{43696600,"CCitadelFamiliarClonePlayerPawn",1},
{43699536,"CCitadel_Ability_Familiar_CloneSingle_Trigger",4},
{43702936,"CCitadel_Ability_Familiar_CloneSingle",4},
{43706512,"CCitadel_Ability_Familiar_Clone",4},
{43709968,"CCitadel_Ability_Familiar_Clone_End",4},
{43714128,"CCitadel_Ability_Fencer_ThrowBlade",4},
{43717704,"CCitadel_Ability_Fencer_Riposte",4},
{43721112,"CCitadel_Ability_RiposteTargetSelect",4},
{43724688,"CCitadel_Ability_Fencer_PrimaryWeapon",4},
{43728512,"CAbility_Fencer_Lunge",4},
{43732224,"CAbility_Fencer_Ultimate",4},
{43736592,"CCitadel_Ability_Fortuna_Ability01",4},
{43739992,"CCitadel_Ability_Fortuna_Ability02",4},
{43743392,"CCitadel_Ability_Fortuna_Ability03",4},
{43746792,"CCitadel_Ability_Fortuna_Ability04",4},
{43750192,"CCitadel_Projectile_FortunaWeapon",6},
{43752480,"CCitadel_Ability_Fortuna_PrimaryWeapon",4},
{43756360,"CCitadel_Ability_Frank_ShockTarget2",4},
{43760064,"CCitadel_Ability_Frank_SelfZap",4},
{43763752,"CCitadel_Ability_Frank_PainAura",4},
{43767328,"CCitadel_Ability_Frank_Revive",4},
{43770904,"CCitadel_Ability_Frank_PrimaryWeapon",4},
{43775200,"CCitadel_Ability_GenericPerson_1",4},
{43778776,"CCitadel_Ability_GenericPerson_2",4},
{43782352,"CCitadel_Ability_GenericPerson_3",4},
{43785928,"CCitadel_Ability_GenericPerson_4",4},
{43789328,"C_Citadel_Projectile_BloodBomb",6},
{43791672,"CCitadel_Ability_BloodBomb",4},
{43795304,"CCitadel_Ability_LifeDrain",4},
{43798704,"CCitadel_Ability_DeathTax",4},
{43802336,"CCitadel_Ability_Ghost_BloodShards",4},
{43806024,"CCitadel_Ability_HealthSwap",4},
{43809968,"CCitadel_Ability_StaticCharge",4},
{43813544,"CCitadel_Ability_StaticCharge_V2",4},
{43817176,"CCitadel_Ability_LightningBall",4},
{43820808,"CCitadel_Ability_StormCloud",4},
{43824440,"CCitadel_Ability_PowerSurge",4},
{43828024,"CCitadel_Ability_Graf_Ability01",4},
{43831424,"CCitadel_Ability_Graf_Ability02",4},
{43834824,"CCitadel_Ability_Graf_Ability03",4},
{43838224,"CCitadel_Ability_Graf_Ability04",4},
{43841912,"CCitadel_Ability_RapidFire",4},
{43845312,"CCitadel_Projectile_RocketLauncher_Rocket",6},
{43847600,"CCitadel_Ability_RocketLauncher",4},
{43851000,"CCitadel_Ability_Tenacity",4},
{43854632,"CCitadel_Ability_SleepBomb",4},
{43858264,"CCitadel_Ability_EmpowerBullet",4},
{43861896,"CCitadel_Ability_Gunslinger_SpreadingFire",4},
{43865472,"CCitadel_Ability_Gunslinger_DemonCarbine",4},
{43869160,"CCitadel_Ability_Gunslinger_Salvo",4},
{43872816,"CCitadel_Gunslinger_DemonMark",4},
{43876448,"CCitadel_Ability_Gunslinger_KnockbackBlast",4},
{43880392,"CCitadel_Ability_Haze_StackingDamage",4},
{43884136,"CCitadel_Ability_SleepDagger",4},
{43887712,"CCitadel_Ability_SmokeBomb",4},
{43891400,"CCitadel_Ability_BulletFlurry",4},
{43895096,"CCitadel_Ability_HornetLeap",4},
{43898728,"CCitadel_Ability_Hornet_Snipe",4},
{43902304,"CCitadel_Ability_HornetSting",4},
{43905880,"CCitadel_Ability_Hornet_Chain",4},
{43909920,"CCitadel_Ability_FireBomb",4},
{43913720,"CCitadel_Ability_FlameDash",4},
{43917368,"CCitadel_Ability_IncendiaryProjectile",4},
{43920768,"CCitadel_Ability_Afterburn",4},
{43924456,"C_CitadelBoomerangProjectile",6},
{43926744,"CCitadel_Ability_Spinning_Blade",4},
{43930320,"CCitadel_Ability_Dust_Storm",4},
{43933720,"C_Citadel_Projectile_DustStorm",6},
{43936064,"CCitadel_Ability_Disruptive_Charge",4},
{43939872,"CCitadel_Ability_Trappers_Bolo",4},
{43946192,"CCitadel_Ability_IceGrenade",4},
{43952200,"CCitadel_Ability_IceDome",4},
{43956008,"CCitadel_Ability_IcePath",4},
{43959752,"CCitadel_Ability_IceBeam",4},
{43963640,"CCitadel_Ability_Lash",4},
{43967216,"CCitadel_Ability_Lash_Flog",4},
{43970848,"CCitadel_Ability_LashDownStrike",4},
{43974720,"CCitadel_Ability_Lash_Ultimate",4},
{44188352,"CCitadel_Ability_MageWalk",4},
{44192040,"CCitadel_Ability_Magician_AnimalHexArea",4},
{44195616,"CCitadel_Ability_Magician_AnimalCurse",4},
{44201704,"CCitadel_Ability_TurretClone_Trigger",4},
{44205104,"CCitadel_Ability_TurretClone",4},
{44208736,"CCitadel_Ability_Magician_ShadowClone",4},
{44212136,"CCitadel_Projectile_MagicBolt",6},
{44214424,"CCitadel_Ability_Magician_MagicBolt",4},
{44218096,"CCitadel_Ability_Magician_BigBolt",4},
{44221672,"CCitadel_Ability_Magician_Escape",4},
{44225248,"CCitadel_Ability_Magician_CopyUlt",4},
{44229608,"CAbility_Mirage_Tornado",4},
{44233464,"CAbility_Mirage_SandPhantom",4},
{44237152,"CCitadel_Ability_Mirage_FireBeetles",4},
{44240784,"CCitadel_Ability_Mirage_Teleport",4},
{44246936,"C_Projectile_Perched_Predator",6},
{44249048,"CCitadel_Ability_Perched_Predator",4},
{44252624,"C_Projectile_Rolling_FireBall",6},
{44254736,"CCitadel_Ability_Rolling_FireBall",4},
{44260664,"CCitadel_Ability_ProximityRitual",4},
{44264496,"CCitadel_Ability_Nano_Pounce",4},
{44268288,"CCitadel_Ability_Nano_Pounce_Instant",4},
{44271872,"CCitadel_Ability_Nano_Shadow",4},
{44275560,"CCitadel_Ability_Nano_CatForm",4},
{44279136,"CCitadel_Ability_Nano_CatFormPounce",4},
{44282712,"CCitadel_Ability_Nano_ClusterGrenade",4},
{44286288,"CCitadel_Ability_NanoDash",4},
{44289864,"CCitadel_Ability_ShadowPulse",4},
{44293440,"CCitadel_Ability_Nano_PrimaryWeapon",4},
{44298056,"CCitadel_Ability_Necro_HauntingSkull",4},
{44303784,"CCitadel_Ability_Necro_KillSummon",4},
{44307384,"CCitadel_Ability_Necro_KillSummonTrigger",4},
{44310960,"CCitadel_Ability_Necro_PrimaryWeapon",4},
{44314728,"CAI_NPC_NecroSkeleVData",5},
{44314784,"CNPC_NecroSkele",5},
{44317584,"CCitadel_Ability_Necro_NukeMap",4},
{44323560,"CCitadel_Ability_Necro_GraveStone",4},
{44327128,"C_Projectile_GraveStone_Projectile",6},
{44329416,"CCitadel_Ability_Necro_ZombieWall",4},
{44332816,"C_Projectile_Necro_ZombieWall_Projectile",6},
{44335216,"CCitadel_Ability_Necro_Coffin",4},
{44338848,"CCitadel_Ability_Necro_Fear",4},
{44342304,"CProjectile_Necro_HauntProjectile",6},
{44344648,"CCitadel_Ability_Necro_HauntingSpirits",4},
{44349528,"CCitadel_Ability_Nurse_LeechTherapy",4},
{44352928,"CAI_NPC_NurseLeechVData",5},
{44352984,"CNPC_NurseLeech",5},
{44355784,"CCitadel_Ability_Nurse_Fear",4},
{44359440,"CCitadel_Ability_Nurse_HealingNeedles",4},
{44363072,"CCitadel_Ability_Nurse_HealingNeedleWeapon",4},
{44367064,"CCitadel_Ability_Nurse_AdrenalineRush",4},
{44371816,"CCitadel_Ability_Opera_Ability01",4},
{44375216,"CCitadel_Ability_Opera_Ability02",4},
{44378616,"CCitadel_Ability_Opera_Ability03",4},
{44382016,"CCitadel_Ability_Opera_Ability04",4},
{44385592,"CAbility_Operative_UmbrellaManeuver",4},
{44389224,"CCitadel_Ability_Operative_Blindside",4},
{44392800,"CAbility_Operative_Revelation",4},
{44396664,"CCitadel_Ability_Priest_Knockback",4},
{44400264,"CCitadel_Ability_Priest_SelfHeal",4},
{44403896,"CCitadel_Ability_Priest_StackingDefense",4},
{44407472,"CCitadel_Ability_Priest_WeaponSwap",4},
{44411048,"CCitadel_Ability_Priest_CrossbowWeapon",4},
{44417304,"CCitadel_Ability_Priest_SmokeGrenade",4},
{44420880,"CCitadel_Ability_Priest_AntiSpiritVest",4},
{44424456,"CCitadel_Ability_Priest_SilenceBomb",4},
{44428144,"CCitadel_Ability_Priest_Flashbang",4},
{44431720,"CCitadel_Ability_Priest_BearTrap",4},
{44437752,"CCitadel_Ability_Punkgoat_PrimaryWeapon",4},
{44441576,"CCitadel_Ability_PunkGoat_Blasted",4},
{44445264,"CCitadel_Ability_PunkGoat_GoatFlip",4},
{44448840,"CCitadel_Ability_PunkGoat_Ult",4},
{44452632,"CCitadel_Ability_PunkGoat_Tether",4},
{44456032,"C_Projectile_PunkgoatTether",6},
{44459272,"CCitadel_Ability_Ratking_PrimaryWeapon",4},
{44463128,"CScrapGrenadeProjectile",6},
{44465240,"CCitadel_Ability_Ratking_ScrapGrenade",4},
{44468640,"CAI_NPC_Ratking_RatVData",5},
{44468696,"CNPC_Ratking_Rat",5},
{44471264,"CCitadel_Ability_RatKing_RatNibble",4},
{44474896,"CCitadel_Ability_RatArmor",4},
{44478472,"CCitadel_Ability_Ratking_EnterTunnel",4},
{44484616,"CCitadel_Ability_Ratking_StandardBearer",4},
{44488688,"C_CProjectile_Rutger_Rocket",6},
{44490800,"CAbility_Rutger_RocketLauncher",4},
{44494376,"CAbility_Rutger_ForceField",4},
{44498008,"CAbility_Rutger_CheatDeath",4},
{44501696,"CCitadel_Ability_Rutger_Pulse",4},
{44505216,"CCitadel_Ability_ShieldGuy_Ability01",4},
{44508616,"CCitadel_Ability_ShieldGuy_Ability02",4},
{44512016,"CCitadel_Ability_ShieldGuy_Ability03",4},
{44515416,"CCitadel_Ability_ShieldGuy_Ability04",4},
{44518992,"CCitadel_Ability_ShivWeapon",4},
{44522816,"CCitadel_Ability_ShivDagger",4},
{44526448,"CCitadel_Ability_ShivDash",4},
{44530064,"CCitadel_Ability_Shiv_Defer_Damage",4},
{44534056,"CCitadel_Ability_Shiv_KillingBlow",4},
{44538280,"CCitadel_Ability_SkyRunner_FlakShot",4},
{44544232,"CCitadel_Ability_Skyrunner_MagicBeam",4},
{44547864,"CCitadel_Ability_SkyRunner_SwingLine",4},
{44551264,"CCitadel_Ability_SkyRunner_Ability04",4},
{44554840,"CCitadel_Ability_SkyRunner_PrimaryWeapon",4},
{44558960,"CCitadel_Ability_Fathom_ScaldingSpray",4},
{44562536,"CCitadel_Ability_Fathom_Breach",4},
{44566128,"CAbility_Fathom_ReefdwellerHarpoon",4},
{44569832,"CAbility_Fathom_LurkersAmbush",4},
{44573696,"CCitadel_Ability_Stomp",4},
{44577096,"C_Projectile_Stomp_Projectile",6},
{44579496,"CCitadel_Ability_VoidSphere",4},
{44583184,"CCitadel_Ability_ChargedTackle",4},
{44586872,"CCitadel_Ability_SelfVacuum",4},
{44590504,"CCitadel_Ability_Nikuman",4},
{44594056,"CCitadel_Projectile_FeatherBoomerang",6},
{44596344,"CCitadel_Ability_Swan_FeatherBoomerang",4},
{44599920,"CCitadel_Ability_Swan_Leap",4},
{44603552,"CCitadel_Ability_Swan_Acrobat",4},
{44606952,"CCitadel_Ability_Swan_Ability04",4},
{44760152,"C_TrackedProjectile_Synth_PlasmaFlux",6},
{44762440,"CAbility_Synth_PlasmaFlux",4},
{44765840,"CAbility_Synth_PlasmaFlux_Trigger",4},
{44769472,"CAbility_Synth_Pulse",4},
{44773160,"C_Projectile_Synth_Barrage",6},
{44775272,"CAbility_Synth_Barrage",4},
{44778928,"CAbility_Synth_Affliction",4},
{44782720,"CCitadel_Ability_Targetdummy_1",4},
{44786296,"CCitadel_Ability_Targetdummy_2",4},
{44789872,"CCitadel_Ability_Targetdummy_3",4},
{44793448,"CCitadel_Ability_Targetdummy_4",4},
{44797080,"CCitadel_Ability_Tengu_Urn",4},
{44800656,"CCitadel_Ability_Tengu_StoneForm",4},
{44804472,"CCitadel_Ability_TangoTether",4},
{44807872,"CCitadel_Ability_TangoTether_Trigger",4},
{44811448,"CCitadel_Ability_Tengu_AirLift",4},
{44815472,"CCitadel_Ability_TestHero_WallCling",4},
{44819048,"CCitadel_Ability_TestHero_SummonCannon",4},
{44822448,"CCitadel_Ability_TestHero_FearTarget",4},
{44825848,"CCitadel_Ability_TestHero_Ability04",4},
{44831760,"CCitadel_Ability_TestHero_SummonSoldier",4},
{44835344,"CCitadel_Ability_TestHero_StanceSwitch",4},
{44838992,"CCitadel_Ability_TestHero_Stance_Spaceman",4},
{44842584,"CCitadel_Ability_TestHero_Stance_Bow",4},
{44846248,"CAbility_TestHero_SpookyHide",4},
{44850024,"CCitadel_Ability_Thumper_1",4},
{44853656,"CCitadel_Ability_Thumper_2",4},
{44857288,"CCitadel_Ability_Thumper_3",4},
{44860864,"CCitadel_Ability_Thumper_4",4},
{44864376,"CCitadel_Projectile_Cyclone",6},
{44866760,"CCitadel_Ability_Trapper_PoisonJar",4},
{44870448,"CCitadel_Ability_Trapper_Fear",4},
{44876272,"CCitadel_Projectile_SpiderProjectile",6},
{44878560,"CCitadel_Ability_Trapper_SpiderJar",4},
{44882192,"CCitadel_Ability_SpiderShield",4},
{44885768,"CCitadel_Projectile_WebWall",6},
{44887880,"CCitadel_Ability_Trapper_WebWall",4},
{44891896,"CCitadel_Ability_Unicorn_PrimaryWeapon",4},
{44895824,"CCitadel_Ability_Unicorn_RadiantBlast",4},
{44899456,"CCitadel_Ability_Unicorn_PrismaticGuard",4},
{44903088,"CCitadel_Ability_Unicorn_DazzlingOrb",4},
{44906816,"CCitadel_Ability_Unicorn_LuminousStrike",4},
{44910704,"CCitadel_Ability_VampireBat_DoubleDagger",4},
{44914288,"CCitadel_Ability_VampireBat_BatBlink",4},
{44917864,"CCitadel_Ability_VampireBat_LoveBites",4},
{44921496,"CCitadel_Ability_VampireBat_BatSwarm",4},
{44924928,"CCitadel_Projectile_BatSwarmProjectile",6},
{44927040,"CCitadel_Projectile_BatSwarmExtraProjectile",6},
{44929440,"CCitadel_Ability_VampireBat_BatCloud",4},
{44933016,"CCitadel_Ability_VampireBat_StealLife",4},
{44937296,"CCitadel_Ability_VandalSurge",4},
{44940752,"CCitadel_Projectile_Pillar",6},
{44943040,"CCitadel_Ability_Vandal_Pillar",4},
{44946440,"CCitadel_Ability_Vandal_Ability03",4},
{44950184,"CCitadel_Ability_VandalOverflow",4},
{44953624,"CCitadel_Projectile_HookBlade",6},
{44955912,"CCitadel_Ability_ViperHookblade",4},
{44959488,"CCitadel_Ability_SnakeDash",4},
{44963176,"CCitadel_Ability_ViperVenom",4},
{44966688,"CCitadel_Projectile_Petrify",6},
{44968976,"CCitadel_Ability_Viper_Ability04",4},
{44972552,"CCitadel_Ability_Viper_DebuffDagger",4},
{44976128,"CCitadel_Ability_Viper_PetrifyBola",4},
{44979864,"C_Citadel_Projectile_Viscous_GooGrenade",6},
{44982152,"CCitadel_Ability_GooGrenade",4},
{44988160,"CCitadel_Ability_RestorativeGoo",4},
{44993936,"CCitadel_Ability_GooBowlingBall",4},
{44997856,"CCitadel_Ability_Viscous_Telepunch",4},
{45002328,"CCitadel_Ability_LockDown",4},
{45005960,"CCitadel_Ability_HighAlert",4},
{45009592,"CCitadel_Ability_CrowdControl",4},
{45013336,"CCitadel_Ability_RiotProtocol",4},
{45017000,"CCitadel_Werewolf_Transformation",4},
{45020744,"CCitadel_Ability_Werewolf_TrackingBomb",4},
{45024320,"CCitadel_Ability_Werewolf_NetShot",4},
{45027952,"CCitadel_Ability_Werewolf_MaulingLeap",4},
{45031528,"CCitadel_Werewolf_Hunt",4},
{45035120,"CCitadel_Werewolf_UnloadGun",4},
{45038712,"CCitadel_Werewolf_CripplingSlash",4},
{45042288,"CCitadel_Ability_Werewolf_ClawWeapon",4},
{45046112,"CCitadel_Ability_Werewolf_Rifle",4},
{45049936,"CCitadel_Ability_Werewolf_KickFlip",4},
{45053680,"CCitadel_Ability_Werewolf_OnTheHunt",4},
{45057256,"CAbility_Werewolf_Frenzy",4},
{45060832,"CCitadel_Werewolf_UnloadGun2",4},
{45064464,"CCitadel_Ability_Werewolf_Leap",4},
{45068992,"CCitadel_Ability_PsychicLift",4},
{45072624,"CCitadel_Ability_ProjectMind",4},
{45076256,"CCitadel_Ability_Wraith_RapidFire",4},
{45079832,"CCitadel_Ability_CardToss",4},
{45083688,"C_Citadel_Projectile_WreckingBall",6},
{45085976,"CCitadel_Ability_WreckingBall",4},
{45089376,"CCitadel_Ability_WreckingBallThrow",4},
{45092952,"CCitadel_Ability_Wrecker_Salvage",4},
{45096696,"CCitadel_Ability_WreckerScrapBlast",4},
{45100272,"CCitadel_Ability_Wrecker_Ultimate",4},
{45104016,"C_Citadel_Projectile_Wrecker_Teleport",6},
{45106128,"CCitadel_Ability_WreckerTeleport",4},
{45109816,"CCitadel_Ability_WreckerGarbageSuck",4},
{45113392,"CCitadel_Ability_Wrecker_BoulderGrenade",4},
{45117056,"CCitadel_Ability_Yakuza_Shakedown",4},
{45120632,"CCitadel_Ability_Shakedown_Target",4},
{45124088,"C_Citadel_Projectile_SettingSun",6},
{45126432,"CCitadel_Ability_SettingSun",4},
{45130008,"CCitadel_Ability_Kobun",4},
{45133640,"CCitadel_Ability_GangActivity",4},
{45137216,"CCitadel_Ability_GangActivity_Cancel",4},
{45140616,"CNPC_YakuzaGangster",5},
{45143360,"CCitadel_Ability_SummonGangster",4},
{45146936,"CCitadel_Ability_TeleportToGangster",4},
{45150568,"CCitadel_Ability_Fealty",4},
{45154144,"CCitadel_Ability_Protection_Racket",4},
{45157968,"CCitadel_Ability_PowerSlash",4},
{45161600,"CCitadel_Ability_FlyingStrike",4},
{45165176,"CCitadel_Ability_HealingSlash",4},
{45168784,"CCitadel_Ability_InfinitySlash",4},
{46095096,"C_BreakableProp",7},
{46251680,"CAI_BaseNPCGraphVariationUserData",5},
{46251760,"?$CBaseConcreteToolAttr@V?$CUtlVector@VCGlobalSymbol@@HV?$CUtlVectorMemory_Growable@VCGlobalSymbol@@H$0A@@@@@VCAI_BaseNPCGraphVariationUserData@@",5},
{46252184,"?$CToolAttrDirectMemberAccess@V?$CUtlVector@VCGlobalSymbol@@HV?$CUtlVectorMemory_Growable@VCGlobalSymbol@@H$0A@@@@@VCAI_BaseNPCGraphVariationUserData@@",5},
{46252232,"?$CConcreteToolAttrDirectMember@V?$CUtlVector@VCGlobalSymbol@@HV?$CUtlVectorMemory_Growable@VCGlobalSymbol@@H$0A@@@@@VCAI_BaseNPCGraphVariationUserData@@",5},
{46253928,"?$CDefaultToolAttrTableForStruct@VCAI_BaseNPCGraphVariationUserData@@",5},
{46254048,"CAI_BaseNPCGraphController",5},
{46254344,"C_AI_MotorServices",5},
{46254464,"C_AI_BaseNPC",5},
{46257232,"CArrayElementAccess@?$CConcreteToolAttr@V?$CUtlVector@VCGlobalSymbol@@HV?$CUtlVectorMemory_Growable@VCGlobalSymbol@@H$0A@@@@@VCAI_BaseNPCGraphVariationUserData@@X",5},
{46257280,"?$CBaseConcreteToolAttr@VCGlobalSymbol@@VCAI_BaseNPCGraphVariationUserData@@",5},
{46257704,"?$CConcreteToolAttr@VCGlobalSymbol@@VCAI_BaseNPCGraphVariationUserData@@X",5},
{46492224,"CNPCMaterialOverrideModifier",5},
{47291200,"C_CitadelProjectile",6},
{47294944,"C_CitadelConfigurableTrackedProjectile",6},
{47304584,"CNPC_FamiliarHelper",5},
{47307152,"CCitadel_Ability_Familiar_HelpingHands",4},
};
inline constexpr uint32_t entities_instruction=25193611, entities_rva=67009680;
inline constexpr uint32_t view_instruction=27282515, view_rva=65739488;
inline constexpr uint32_t rules_instruction=26824779, rules_rva=65702848;
inline constexpr uint32_t engine2_timestamp=1792525213u,engine2_image_size=9461760u;
inline constexpr uint32_t engine_ui=6006672,engine_ui_vtable=5246400,engine_ui_flags=0x60;
inline constexpr uint32_t inputsystem_timestamp=1792524832u,inputsystem_image_size=339968u;
inline constexpr uint32_t input_instance=289728,input_vtable=234464,virtual_key_table=317488,input_buttons=0x12b8;
}
