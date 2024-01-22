#pragma once

#include "IPlugin.h"
#include <cstdint>
#include <ranges>

class openRBRVR : public IPlugin {
    IRBRGame* game;

public:
    openRBRVR(IRBRGame* g);
    virtual ~openRBRVR(void);
    virtual const char* GetName(void) { return "openRBRVR"; }
    virtual void DrawFrontEndPage(void);
    virtual void DrawResultsUI(void) {};
    virtual void HandleFrontEndEvents(char txtKeyboard, bool bUp, bool bDown, bool bLeft, bool bRight, bool bSelect);
    virtual void TickFrontEndPage(float fTimeDelta) { }
    virtual void StageStarted(int iMap, const char* ptxtPlayerName, bool bWasFalseStart) { }
    virtual void HandleResults(float fCheckPoint1, float fCheckPoint2, float fFinishTime, const char* ptxtPlayerName) { }
    virtual void CheckPoint(float fCheckPointTime, int iCheckPointID, const char* ptxtPlayerName) { }

    void DrawMenuEntries(const std::ranges::forward_range auto& entries, float rowHeight = 21.0f);
};
