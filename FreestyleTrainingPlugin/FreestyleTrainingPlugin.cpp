#include "FreestyleTrainingPlugin.h"

#include "bakkesmod\wrappers\GameEvent\TutorialWrapper.h"
#include "bakkesmod\wrappers\GameObject\CarWrapper.h"
#include "bakkesmod\wrappers\GameObject\BallWrapper.h"

#define SPINYOUBITCHDEADZONE 0.2f

BAKKESMOD_PLUGIN(FreestyleTrainingPlugin, "Freestyle Training Plugin", "0.1", PLUGINTYPE_FREEPLAY)


static inline float random(float min, float max) {
	return min + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (max - min)));
}

void FreestyleTrainingPlugin::onLoad()
{
	cvarManager->log("FreestyleTrainingPlugin::onLoad");

	enabled = make_shared<bool>(false);
	autoAirRoll = make_shared<float>(0.f);

	randomizeAutoAirRollEnabled = make_shared<bool>(false);
	randomizeAutoAirRollLower = make_shared<float>(0.0f);
	randomizeAutoAirRollUpper = make_shared<float>(0.0f);

	// freeplay is now a mutator
	gameWrapper->HookEvent("Function TAGame.Mutator_Freeplay_TA.Init", bind(&FreestyleTrainingPlugin::OnFreeplayLoad, this, std::placeholders::_1));
	gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.Destroyed", bind(&FreestyleTrainingPlugin::OnFreeplayDestroy, this, std::placeholders::_1));

	cvarManager->registerCvar("freestyletraining_enabled", "0", "Enables/disable freestyle training mode", true, true, 0.f, true, 1.f)
		.addOnValueChanged(std::bind(&FreestyleTrainingPlugin::OnEnabledChanged, this, std::placeholders::_1, std::placeholders::_2));
	cvarManager->getCvar("freestyletraining_enabled").bindTo(enabled);

	cvarManager->registerCvar("freestyletraining_auto_air_roll", "0", "Auto air roll amount", true, true, -1.f, true, 1.f).bindTo(autoAirRoll);

	cvarManager->registerCvar("freestyletraining_randomize_auto_air_roll_enabled", "0", "Enables/disable auto air roll randomization", true, true, 0.f, true, 1.f).bindTo(randomizeAutoAirRollEnabled);
	cvarManager->registerCvar("freestyletraining_randomize_auto_air_roll_lower", "0", "Random Auto Air Roll Lower", true, true, -1.f, true, 1.f).bindTo(randomizeAutoAirRollLower);
	cvarManager->registerCvar("freestyletraining_randomize_auto_air_roll_upper", "0", "Random Auto Air Roll Upper", true, true, -1.f, true, 1.f).bindTo(randomizeAutoAirRollUpper);

	cvarManager->registerNotifier("freestyletraining_do_randomize_auto_air_roll", std::bind(&FreestyleTrainingPlugin::DoRandomizeAutoAirRoll, this), "Randomize auto air roll within upper and lower", PERMISSION_FREEPLAY);
}


void FreestyleTrainingPlugin::onUnload()
{
	cvarManager->log("FreestyleTrainingPlugin::onUnload");
}

// todo: rename since is not hooked OnPreAsync, was a copy paste mistake
void FreestyleTrainingPlugin::OnPreAsync(CarWrapper cw, void * params, string funcName) 
{
	//cvarManager->log("FreestyleTrainingPlugin::OnPreAsync");
	
	if (!gameWrapper->IsInFreeplay())
	{
		cvarManager->log("not in freeplay but freestyle training still hooked somehow");
		return;
	}

	ControllerInput* ci = (ControllerInput*)params;

	// if user is not overriding with their own inputs
	if (abs(ci->Roll) < SPINYOUBITCHDEADZONE) {
		ci->Roll = std::max(-1.0f, std::min(*autoAirRoll, 1.0f));
		RandomizeAirRollTick(true);
		// cvarManager->log("air roll overridden to:" + to_string(ci->Roll));
	}
	else 
	{
		RandomizeAirRollTick(false);
	}
	
	//cvarManager->log("auto set roll to:" + to_string(ci->Roll));
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
	cvarManager->log("FreestyleTrainingPlugin::OnEnabledChanged oldValue:" + oldValue + " current:" + to_string(cvar.getBoolValue()));
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
	// copied from mechanical plugin
	gameWrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.SetVehicleInput",
		bind(&FreestyleTrainingPlugin::OnPreAsync, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void FreestyleTrainingPlugin::Unhook() 
{
	cvarManager->log("FreestyleTrainingPlugin::Unhook");
	gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
}

void FreestyleTrainingPlugin::RandomizeAirRollTick(bool usedAutoAirRollThisTick)
{
	// randomize auto air roll amount each time user finishes overriding
	
	// cvarManager->log("RandomizeAirRollTick usedAutoAirRollLastTick:" + to_string(usedAutoAirRollLastTick) + " usedAutoAirRollThisTick:" + to_string(usedAutoAirRollThisTick) + " randomizeAutoAirRollEnabled:" + to_string(*randomizeAutoAirRollEnabled));

	if (usedAutoAirRollLastTick && !usedAutoAirRollThisTick && *randomizeAutoAirRollEnabled)
	{
		DoRandomizeAutoAirRoll();
	}

	usedAutoAirRollLastTick = usedAutoAirRollThisTick;
}

void FreestyleTrainingPlugin::DoRandomizeAutoAirRoll()
{
	float randomMin = min(*randomizeAutoAirRollLower, *randomizeAutoAirRollUpper);
	float randomMax = max(*randomizeAutoAirRollLower, *randomizeAutoAirRollUpper);

	float randomized = random(randomMin, randomMax);

	//cvarManager->log("DoRandomizeAutoAirRoll randomMin: " + to_string(randomMin) + " randomMax:" + to_string(randomMax) + " randomized:" + to_string(randomized));

	cvarManager->getCvar("freestyletraining_auto_air_roll").setValue(randomized);
}