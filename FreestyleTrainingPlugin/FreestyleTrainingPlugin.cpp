#define NOMINMAX
#include "FreestyleTrainingPlugin.h"

#include "bakkesmod\wrappers\GameEvent\TutorialWrapper.h"
#include "bakkesmod\wrappers\GameObject\CarWrapper.h"
#include "bakkesmod\wrappers\GameObject\BallWrapper.h"
#include <sstream>
#include <iomanip>

#define SPINYOUBITCHDEADZONE 0.2f

#define RANDOM_OFF 0
#define RANDOM_LEFT 1
#define RANDOM_RIGHT 2

#define CVAR_ENABLED "freestyletraining_enabled"
#define CVAR_AUTO_AIR_ROLL "freestyletraining_auto_air_roll"

#define CVAR_LOG_DIRECTION_CHANGE "freestyletraining_log_direction_change"
#define CVAR_LOG_GRAVITY_CHANGE "freestyletraining_log_gravity_change"

#define CVAR_RANDOM_AUTO_AIR_ROLL_ENABLED "freestyletraining_randomize_auto_air_roll_enabled"
#define CVAR_RANDOM_AUTO_AIR_ROLL_AFTER_INPUT "freestyletraining_randomize_auto_air_roll_after_input"
#define CVAR_RANDOM_AUTO_AIR_ROLL_AMOUNT "freestyletraining_randomize_auto_air_roll_amount"
#define CVAR_RANDOM_AUTO_AIR_ROLL_TIME "freestyletraining_randomize_auto_air_roll_time"

#define CVAR_RANDOM_GRAVITY_ENABLED "freestyletraining_randomize_gravity_enabled"
#define CVAR_RANDOM_GRAVITY_AMOUNT "freestyletraining_randomize_gravity_amount"
#define CVAR_RANDOM_GRAVITY_TIME "freestyletraining_randomize_gravity_time"

#define TRANSITION_NUM_STEPS 13
#define TRANSITION_SECONDS 0.65f


BAKKESMOD_PLUGIN(FreestyleTrainingPlugin, "Freestyle Training Plugin", "0.2", PLUGINTYPE_FREEPLAY)


static inline float random(float min, float max) {
	return min + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (max - min)));
}

void FreestyleTrainingPlugin::onLoad()
{
	cvarManager->log("FreestyleTrainingPlugin::onLoad");

	srand(time(NULL));

	enabled = make_shared <bool>(false);
	autoAirRoll = make_shared<int>(0);

	randomizeAutoAirRollEnabled = make_shared<int>(RANDOM_OFF);
	randomizeAutoAirRollAmount = make_shared<int>(0);

	// freeplay is now a mutator
	gameWrapper->HookEvent("Function TAGame.Mutator_Freeplay_TA.Init", bind(&FreestyleTrainingPlugin::OnFreeplayLoad, this, std::placeholders::_1));
	gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.Destroyed", bind(&FreestyleTrainingPlugin::OnFreeplayDestroy, this, std::placeholders::_1));

	cvarManager->registerCvar(CVAR_ENABLED, "0", "Enables/disable freestyle training mode", true, true, 0.f, true, 1.f)
		.addOnValueChanged(std::bind(&FreestyleTrainingPlugin::OnEnabledChanged, this, std::placeholders::_1, std::placeholders::_2));
	cvarManager->getCvar(CVAR_ENABLED).bindTo(enabled);

	cvarManager->registerCvar(CVAR_LOG_DIRECTION_CHANGE, "1", "Notify on direction change", true, true, 0.f, true, 1.f);
	cvarManager->registerCvar(CVAR_LOG_GRAVITY_CHANGE, "1", "Notify on direction change", true, true, 0.f, true, 1.f);

	cvarManager->registerCvar(CVAR_AUTO_AIR_ROLL, "0", "Auto air roll amount", true, true, -100, true, 100).bindTo(autoAirRoll);

	// todo: combine enabled and direction 0 = off, 1 = left, 2 = right, 3 = left and right. then can use bool ops and look cool
	cvarManager->registerCvar(CVAR_RANDOM_AUTO_AIR_ROLL_ENABLED, "0", "Enables/disable auto air roll randomization", true, true, 0, true, 3).bindTo(randomizeAutoAirRollEnabled);
	cvarManager->getCvar(CVAR_RANDOM_AUTO_AIR_ROLL_ENABLED).addOnValueChanged([this](string oldValue, CVarWrapper cvar)
	{
		int newRandomEnabled = cvar.getIntValue();
		Log("newRandomEnabled:" + to_string(newRandomEnabled), true);

		if (newRandomEnabled) {
			string direction;
			switch (newRandomEnabled) {
			case RANDOM_LEFT:
				direction = "Left Only";
				break;
			case RANDOM_RIGHT:
				direction = "Right Only";
				break;
			case RANDOM_LEFT | RANDOM_RIGHT:
				direction = "Left & Right";
				break;
			}

			Log("Random Auto Air Roll ON - " + direction, true);
			DoRandomizeAutoAirRoll();
		}
		else {
			Log("Random Auto Air Roll OFF", true);
		}
	});

	cvarManager->registerCvar(CVAR_RANDOM_AUTO_AIR_ROLL_AFTER_INPUT, "1", "Randomize Auto Air Roll After Manual Input", true, true, 0.f, true, 1.f);

	cvarManager->registerCvar(CVAR_RANDOM_AUTO_AIR_ROLL_AMOUNT, "(0, 100)", "Random Auto Air Roll Amount", true, true, 0, true, 100).bindTo(randomizeAutoAirRollAmount);
	cvarManager->registerCvar(CVAR_RANDOM_AUTO_AIR_ROLL_TIME, "(0, 60)", "Random Auto Air Roll Time", true, true, 0, true, 600);

	cvarManager->registerNotifier("freestyletraining_do_randomize_auto_air_roll", std::bind(&FreestyleTrainingPlugin::DoRandomizeAutoAirRoll, this), "Randomize auto air roll within upper and lower", PERMISSION_FREEPLAY);

	cvarManager->registerCvar(CVAR_RANDOM_GRAVITY_ENABLED, "0", "Enables/disable gravity randomization", true, true, 0.f, true, 1.f);
	cvarManager->registerCvar(CVAR_RANDOM_GRAVITY_AMOUNT, "(-650, 650)", "Random gravity range", true, true, -1000, true, 1000);
	cvarManager->registerCvar(CVAR_RANDOM_GRAVITY_TIME, "(0, 60)", "Random gravity time", true, true, 0, true, 600);

	cvarManager->registerNotifier("freestyletraining_do_randomize_gravity", std::bind(&FreestyleTrainingPlugin::DoRandomizeGravity, this), "Randomize gravity within upper and lower", PERMISSION_FREEPLAY);

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

	if (*enabled)
	{
		cvarManager->log("FreestyleTrainingPlugin not enabled anymore but still hooked");
		return;
	}

	ControllerInput* ci = (ControllerInput*)params;
	ServerWrapper training = gameWrapper->GetGameEventAsServer();

	// if user is not overriding with their own inputs
	if (abs(ci->Roll) < SPINYOUBITCHDEADZONE) {
		ci->Roll = std::max(-1.0f, std::min(*autoAirRoll / 100.0f, 1.0f));
		RandomizeAirRollTick(true);
	}
	else 
	{
		RandomizeAirRollTick(false);
	}

	float secondsElapsed = training.GetSecondsElapsed();

	// push scheduled change out by transition length as a lazy way to not get overlapping transitions
	float randomizedAirRollTimeInRange = cvarManager->getCvar(CVAR_RANDOM_AUTO_AIR_ROLL_TIME).getFloatValue() + TRANSITION_SECONDS;

	if (*randomizeAutoAirRollEnabled && randomizedAirRollTimeInRange > 0 && secondsElapsed > nextAirRollRandomizationTimeout) 
	{
		DoRandomizeAutoAirRoll();
		
		cvarManager->log("randomization timeout elapsed - randomizing auto air roll again in " + to_string(randomizedAirRollTimeInRange) + " seconds");

		nextAirRollRandomizationTimeout = secondsElapsed + randomizedAirRollTimeInRange;
	}

	float randomizedGravityTimeInRange = cvarManager->getCvar(CVAR_RANDOM_GRAVITY_TIME).getFloatValue() + TRANSITION_SECONDS;
	if (cvarManager->getCvar(CVAR_RANDOM_GRAVITY_ENABLED).getBoolValue() && randomizedGravityTimeInRange > 0 && secondsElapsed > nextGravityRandomizationTimeout) 
	{
		DoRandomizeGravity();
		nextGravityRandomizationTimeout = secondsElapsed + randomizedGravityTimeInRange;
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
	nextAirRollRandomizationTimeout = .0f;
	nextGravityRandomizationTimeout = .0f;

	// copied from mechanical plugin
	gameWrapper->HookEventWithCaller<CarWrapper>("Function TAGame.Car_TA.SetVehicleInput",
		bind(&FreestyleTrainingPlugin::SetVehicleInput, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void FreestyleTrainingPlugin::Unhook() 
{
	cvarManager->log("FreestyleTrainingPlugin::Unhook");
	nextAirRollRandomizationTimeout = .0f;
	nextGravityRandomizationTimeout = .0f;
	gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
}

void FreestyleTrainingPlugin::RandomizeAirRollTick(bool usedAutoAirRollThisTick)
{
	// randomize auto air roll amount each time user finishes overriding
	if (usedAutoAirRollLastTick && !usedAutoAirRollThisTick && *randomizeAutoAirRollEnabled && cvarManager->getCvar(CVAR_RANDOM_AUTO_AIR_ROLL_AFTER_INPUT).getBoolValue())
	{
		DoRandomizeAutoAirRoll();
	}

	usedAutoAirRollLastTick = usedAutoAirRollThisTick;
}

void FreestyleTrainingPlugin::DoRandomizeAutoAirRoll()
{
	int randomized = cvarManager->getCvar(CVAR_RANDOM_AUTO_AIR_ROLL_AMOUNT).getIntValue();

	switch (*randomizeAutoAirRollEnabled) {
	case RANDOM_LEFT:
		randomized = -1 * randomized;
		break;
	case RANDOM_RIGHT:
		break;
	case RANDOM_LEFT | RANDOM_RIGHT:
		if (rand() % 2 == 0) {
			// 50% chance of randomizing left
			randomized = -1 * randomized;
		}
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

	Log(chatMsg.str(), cvarManager->getCvar(CVAR_LOG_DIRECTION_CHANGE).getBoolValue());

	float randomDeltaPerStep = (randomized - *autoAirRoll) / (float)TRANSITION_NUM_STEPS;
	int fromAirRoll = *autoAirRoll;
	Log("from:" + to_string(fromAirRoll) + " to:" + to_string(randomized) + " randomDeltaPerStep:" + to_string(randomDeltaPerStep), false);

	for (int i = 0; i < TRANSITION_NUM_STEPS; i++) {
		// todo: make a function for this with schedule future change
		gameWrapper->SetTimeout([this, fromAirRoll, randomDeltaPerStep, i](GameWrapper* gw) {
			Log("transition step: " + to_string(fromAirRoll + (randomDeltaPerStep * i)), false);
			cvarManager->getCvar(CVAR_AUTO_AIR_ROLL).setValue(fromAirRoll + (randomDeltaPerStep * i));
		}, TRANSITION_SECONDS / (float)TRANSITION_NUM_STEPS * i);
	}
	
	gameWrapper->SetTimeout([this, randomized](GameWrapper* gw) {
		Log("transition finished: " + to_string(randomized), false);
		cvarManager->getCvar(CVAR_AUTO_AIR_ROLL).setValue(randomized);
	}, TRANSITION_SECONDS);
	
}

void FreestyleTrainingPlugin::DoRandomizeGravity()
{
	int randomizedGravityAmount = cvarManager->getCvar(CVAR_RANDOM_GRAVITY_AMOUNT).getIntValue();
	Log("Randomizing gravity" + to_string(randomizedGravityAmount), cvarManager->getCvar(CVAR_LOG_GRAVITY_CHANGE).getBoolValue());
	cvarManager->executeCommand("sv_soccar_gravity " + to_string(randomizedGravityAmount), false);
}

void FreestyleTrainingPlugin::Log(string message, bool sendToChat) 
{
	cvarManager->log(message);
	if (sendToChat) {
		gameWrapper->LogToChatbox(message, "BM");
	}
}
