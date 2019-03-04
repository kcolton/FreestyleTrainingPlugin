#include "FreestyleTrainingPlugin.h"

#include "bakkesmod\wrappers\GameEvent\TutorialWrapper.h"
#include "bakkesmod\wrappers\GameObject\CarWrapper.h"
#include "bakkesmod\wrappers\GameObject\BallWrapper.h"

BAKKESMOD_PLUGIN(FreestyleTrainingPlugin, "Freestyle Training Plugin", "0.1", PLUGINTYPE_FREEPLAY)


void FreestyleTrainingPlugin::onLoad()
{
	cvarManager->log("FreestyleTrainingPlugin::onLoad");

	enabled = make_shared<bool>(false);
	autoAirRoll = make_shared<float>(0.f);

	gameWrapper->HookEvent("Function TAGame.GameEvent_Tutorial_TA.OnInit", bind(&FreestyleTrainingPlugin::OnFreeplayLoad, this, std::placeholders::_1));
	gameWrapper->HookEvent("Function TAGame.GameEvent_Tutorial_TA.Destroyed", bind(&FreestyleTrainingPlugin::OnFreeplayDestroy, this, std::placeholders::_1));

	cvarManager->registerCvar("freestyletraining_enabled", "0", "Enables/disable freestyle training mode", true, true, 0.f, true, 1.f)
		.addOnValueChanged(std::bind(&FreestyleTrainingPlugin::OnEnabledChanged, this, std::placeholders::_1, std::placeholders::_2));
	cvarManager->getCvar("freestyletraining_enabled").bindTo(enabled);

	cvarManager->registerCvar("freestyletraining_auto_air_roll", "0", "Auto air roll", true, true, -1.f, true, 1.f).bindTo(autoAirRoll);
}


void FreestyleTrainingPlugin::onUnload()
{
	cvarManager->log("FreestyleTrainingPlugin::onUnload");
}


void FreestyleTrainingPlugin::OnPreAsync(CarWrapper cw, void * params, string funcName) 
{
	cvarManager->log("FreestyleTrainingPlugin::OnPreAsync");
	
	if (!gameWrapper->IsInFreeplay())
	{
		cvarManager->log("not in freeplay but freestyle training still hooked somehow");
		return;
	}

	ControllerInput* ci = (ControllerInput*)params;

	ci->Roll = std::max(-1.0f, std::min(ci->Roll + *autoAirRoll, 1.0f));
	cvarManager->log("auto set roll to:" + to_string(ci->Roll));
}


void FreestyleTrainingPlugin::OnFreeplayLoad(std::string eventName) 
{
	cvarManager->log("FreestyleTrainingPlugin::OnFreeplayLoad eventName:" + eventName);
	if (*enabled)
	{
		Hook();
	}
}

void FreestyleTrainingPlugin::OnFreeplayDestroy(std::string eventName) 
{
	cvarManager->log("FreestyleTrainingPlugin::OnFreeplayDestroy eventName:" + eventName);
	Unhook();
}

void FreestyleTrainingPlugin::OnEnabledChanged(std::string oldValue, CVarWrapper cvar)
{
	cvarManager->log("FreestyleTrainingPlugin::OnEnabledChanged oldValue:" + oldValue);
	if (cvar.getBoolValue() && gameWrapper->IsInFreeplay())
	{
		Hook();
	}
	else
	{
		Unhook();
	}
}

void FreestyleTrainingPlugin::Hook() 
{
	cvarManager->log("FreestyleTrainingPlugin::Hook");
	// copied from mechanical plugin. dont understand fully
	gameWrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.SetVehicleInput",
		bind(&FreestyleTrainingPlugin::OnPreAsync, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void FreestyleTrainingPlugin::Unhook() 
{
	cvarManager->log("FreestyleTrainingPlugin::Unhook");
	// copied from mechanical plugin. dont understand fully
	gameWrapper->UnhookEvent("Function TAGame.RBActor_TA.PreAsyncTick");
}