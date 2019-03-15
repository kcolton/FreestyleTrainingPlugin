#pragma once
#pragma comment( lib, "bakkesmod.lib" )
#include "bakkesmod/plugin/bakkesmodplugin.h"

class FreestyleTrainingPlugin : public BakkesMod::Plugin::BakkesModPlugin
{
private:
	shared_ptr<bool> enabled;
	shared_ptr<float> autoAirRoll;

	shared_ptr<bool> randomizeAutoAirRollEnabled;
	shared_ptr<float> randomizeAutoAirRollLower;
	shared_ptr<float> randomizeAutoAirRollUpper;

	bool usedAutoAirRollLastTick;

	void Hook();
	void Unhook();

	void RandomizeAirRollTick(bool usedAutoAirRollThisTick);

	void DoRandomizeAutoAirRoll();

public:
	virtual void onLoad();
	virtual void onUnload();
	
	void OnPreAsync(CarWrapper cw, void * params, string funcName);
	void OnFreeplayLoad(std::string eventName);
	void OnFreeplayDestroy(std::string eventName);
	void OnEnabledChanged(std::string oldValue, CVarWrapper cvar);
};

