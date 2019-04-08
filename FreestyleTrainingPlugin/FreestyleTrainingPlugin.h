#pragma once
#pragma comment( lib, "bakkesmod.lib" )
#include "bakkesmod/plugin/bakkesmodplugin.h"

class FreestyleTrainingPlugin : public BakkesMod::Plugin::BakkesModPlugin
{
private:
	shared_ptr<bool> enabled;
	shared_ptr<float> autoAirRoll;

	shared_ptr<int> randomizeAutoAirRollEnabled;
	shared_ptr<float> randomizeAutoAirRollAmount;

	bool usedAutoAirRollLastTick;

	float nextRandomizationTimeout = .0f;
	
	void Hook();
	void Unhook();

	void RandomizeAirRollTick(bool usedAutoAirRollThisTick);

	void DoRandomizeAutoAirRoll();

public:
	virtual void onLoad();
	virtual void onUnload();
	
	void SetVehicleInput(CarWrapper cw, void * params, string funcName);
	void OnFreeplayLoad(std::string eventName);
	void OnFreeplayDestroy(std::string eventName);
	void OnEnabledChanged(std::string oldValue, CVarWrapper cvar);
	void Log(std::string message, bool sendToChat);
};

