#include "FreestyleTrainingPlugin.h"

#include "bakkesmod\wrappers\GameEvent\TutorialWrapper.h"
#include "bakkesmod\wrappers\GameObject\CarWrapper.h"
#include "bakkesmod\wrappers\GameObject\BallWrapper.h"
#include <sstream>
#include <iomanip>

#define SPINYOUBITCHDEADZONE 0.2f

#define LEFT_AND_RIGHT 0
#define LEFT 1
#define RIGHT 2

BAKKESMOD_PLUGIN(FreestyleTrainingPlugin, "Freestyle Training Plugin", "0.1", PLUGINTYPE_FREEPLAY)


static inline float random(float min, float max) {
	return min + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (max - min)));
}

void FreestyleTrainingPlugin::onLoad()
{
	cvarManager->log("FreestyleTrainingPlugin::onLoad");

	srand(time(NULL));

	enabled = make_shared<bool>(false);
	autoAirRoll = make_shared<float>(0.f);

	randomizeAutoAirRollEnabled = make_shared<bool>(false);
	randomizeAutoAirRollLower = make_shared<float>(0.0f);
	randomizeAutoAirRollUpper = make_shared<float>(0.0f);
	randomizeAutoAirRollDirection = make_shared<int>(0);

	// freeplay is now a mutator
	gameWrapper->HookEvent("Function TAGame.Mutator_Freeplay_TA.Init", bind(&FreestyleTrainingPlugin::OnFreeplayLoad, this, std::placeholders::_1));
	gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.Destroyed", bind(&FreestyleTrainingPlugin::OnFreeplayDestroy, this, std::placeholders::_1));

	cvarManager->registerCvar("freestyletraining_enabled", "0", "Enables/disable freestyle training mode", true, true, 0.f, true, 1.f)
		.addOnValueChanged(std::bind(&FreestyleTrainingPlugin::OnEnabledChanged, this, std::placeholders::_1, std::placeholders::_2));
	cvarManager->getCvar("freestyletraining_enabled").bindTo(enabled);

	cvarManager->registerCvar("freestyletraining_auto_air_roll", "0", "Auto air roll amount", true, true, -1.f, true, 1.f).bindTo(autoAirRoll);

	// todo: combine enabled and direction 0 = off, 1 = left, 2 = right, 3 = left and right. then can use bool ops and look cool
	cvarManager->registerCvar("freestyletraining_randomize_auto_air_roll_enabled", "0", "Enables/disable auto air roll randomization", true, true, 0.f, true, 1.f).bindTo(randomizeAutoAirRollEnabled);
	cvarManager->getCvar("freestyletraining_randomize_auto_air_roll_enabled").addOnValueChanged([this](string oldValue, CVarWrapper cvar)
	{
		if (cvar.getBoolValue()) {
			string direction;
			switch (*randomizeAutoAirRollDirection) {
			case LEFT_AND_RIGHT:
				direction = "Left & Right";
				break;
			case LEFT:
				direction = "Left Only";
				break;
			case RIGHT:
				direction = "Right Only";
				break;
			}

			Log("Random Auto Air Roll ON - " + direction, true);
			DoRandomizeAutoAirRoll();
		}
		else {
			Log("Random Auto Air Roll OFF", true);
		}
	});

	// todo: use the range widget that i didn't know about at the time
	cvarManager->registerCvar("freestyletraining_randomize_auto_air_roll_lower", "0", "Random Auto Air Roll Lower", true, true, 0.0f, true, 1.f).bindTo(randomizeAutoAirRollLower);
	cvarManager->registerCvar("freestyletraining_randomize_auto_air_roll_upper", "0", "Random Auto Air Roll Upper", true, true, 0.0f, true, 1.f).bindTo(randomizeAutoAirRollUpper);
	cvarManager->registerCvar("freestyletraining_randomize_auto_air_roll_direction", "0", "Random Auto Air Roll Direction", false, false, 0, false, 0, false).bindTo(randomizeAutoAirRollDirection);
	cvarManager->registerCvar("freestyletraining_randomize_auto_air_roll_time", "0", "Random Auto Air Roll Time", true, true, 0.0f, true, 600.0f);

	cvarManager->registerNotifier("freestyletraining_do_randomize_auto_air_roll", std::bind(&FreestyleTrainingPlugin::DoRandomizeAutoAirRoll, this), "Randomize auto air roll within upper and lower", PERMISSION_FREEPLAY);
}


void FreestyleTrainingPlugin::onUnload()
{
	cvarManager->log("FreestyleTrainingPlugin::onUnload");
}

void FreestyleTrainingPlugin::SetVehicleInput(CarWrapper cw, void * params, string funcName)
{	
	if (!gameWrapper->IsInFreeplay())
	{
		cvarManager->log("not in freeplay but freestyle training still hooked somehow");
		return;
	}

	ControllerInput* ci = (ControllerInput*)params;
	ServerWrapper training = gameWrapper->GetGameEventAsServer();

	// if user is not overriding with their own inputs
	if (abs(ci->Roll) < SPINYOUBITCHDEADZONE) {
		ci->Roll = std::max(-1.0f, std::min(*autoAirRoll, 1.0f));
		RandomizeAirRollTick(true);
	}
	else 
	{
		RandomizeAirRollTick(false);
	}

	float secondsElapsed = training.GetSecondsElapsed();
	float randomizedTimeInRange = cvarManager->getCvar("freestyletraining_randomize_auto_air_roll_time").getFloatValue();

	if (*randomizeAutoAirRollEnabled && randomizedTimeInRange > 0 && secondsElapsed > nextRandomizationTimeout) {
		DoRandomizeAutoAirRoll();
		
		cvarManager->log("randomization timeout elapsed - randomizing auto air roll again in " + to_string(randomizedTimeInRange) + "seconds");

		nextRandomizationTimeout = secondsElapsed + randomizedTimeInRange;
	}
}


void FreestyleTrainingPlugin::OnFreeplayLoad(std::string eventName) 
{
	cvarManager->log("FreestyleTrainingPlugin::OnFreeplayLoad eventName:" + eventName);
	if (*enabled)
	{
		gameWrapper->LogToChatbox("Freestyle Training Plugin ON");
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
		gameWrapper->LogToChatbox("Freestyle Training Plugin Enabled");
		Hook();
	}
	else
	{
		gameWrapper->LogToChatbox("Freestyle Training Plugin OFF");
		Unhook();
	}
}

void FreestyleTrainingPlugin::Hook() 
{
	cvarManager->log("FreestyleTrainingPlugin::Hook");
	// copied from mechanical plugin
	gameWrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.SetVehicleInput",
		bind(&FreestyleTrainingPlugin::SetVehicleInput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void FreestyleTrainingPlugin::Unhook() 
{
	cvarManager->log("FreestyleTrainingPlugin::Unhook");
	gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
}

void FreestyleTrainingPlugin::RandomizeAirRollTick(bool usedAutoAirRollThisTick)
{
	// randomize auto air roll amount each time user finishes overriding
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

	float randomized = abs(random(randomMin, randomMax));

	switch (*randomizeAutoAirRollDirection) {
	case LEFT_AND_RIGHT:
		if (rand() % 2 == 0) {
			// 50% chance of randomizing left
			randomized = -1.0f * randomized;
		}
	case LEFT:
		randomized = -1.0f * randomized;
		break;
	case RIGHT:
		break;
	}
	
	// todo: maybe delay this chat until auto air roll takes affect again (may be still overriding)
	stringstream chatMsg;
	chatMsg << "Random Auto Air Roll:";
	chatMsg << setprecision(2) << randomized;

	if (randomized < 0) {
		chatMsg << " [left]";
	}
	else if (randomized > 0) {
		chatMsg << " [right]";
	}

	Log(chatMsg.str(), true);

	cvarManager->getCvar("freestyletraining_auto_air_roll").setValue(randomized);
}

void FreestyleTrainingPlugin::Log(string message, bool sendToChat) 
{
	cvarManager->log(message);
	if (sendToChat) {
		gameWrapper->LogToChatbox(message, "BM");
	}
}
