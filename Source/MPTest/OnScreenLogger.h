// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

enum class EScreenLogLevel{
	INFO,
	WARNING,
	CRITICAL
};

class MPTEST_API OnScreenLogger
{

private:
	inline static TMap<EScreenLogLevel, FColor> LogLevelColor {
		{EScreenLogLevel::INFO, FColor::Blue},
		{EScreenLogLevel::WARNING, FColor::Yellow},
		{EScreenLogLevel::CRITICAL, FColor::Red}
	};

public:
	OnScreenLogger();

	template <typename FmtType, typename... Types>
	static void ScreenLog(EScreenLogLevel OnScreenLogLevel, const FmtType & Fmt, Types... Args) {
		if (GEngine) {
			GEngine->AddOnScreenDebugMessage(
				-1, 15.f, LogLevelColor[OnScreenLogLevel], FString::Printf(Fmt, Args...));
		}
	}

	template <typename FmtType, typename... Types>
	static void ScreenLogInfo(const FmtType & Fmt, Types... Args) {
		OnScreenLogger::ScreenLog(EScreenLogLevel::INFO, Fmt, Args...);
	}

	template <typename FmtType, typename... Types>
	static void ScreenLogWarning(const FmtType & Fmt, Types... Args) {
		OnScreenLogger::ScreenLog(EScreenLogLevel::WARNING, Fmt, Args...);
	}

	template <typename FmtType, typename... Types>
	static void ScreenLogCritical(const FmtType & Fmt, Types... Args) {
		OnScreenLogger::ScreenLog(EScreenLogLevel::CRITICAL, Fmt, Args...);
	}
};
