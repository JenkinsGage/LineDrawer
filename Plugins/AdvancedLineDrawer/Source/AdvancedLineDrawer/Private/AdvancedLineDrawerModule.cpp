// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FAdvancedLineDrawerModule"

class FAdvancedLineDrawerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAdvancedLineDrawerModule, AdvancedLineDrawer)
