#pragma once

class IRBRGame {
public:
    enum ERBRTyreTypes {
        TYRE_TARMAC_DRY,
        TYRE_TARMAC_INTERMEDIATE,
        TYRE_TARMAC_WET,
        TYRE_GRAVEL_DRY,
        TYRE_GRAVEL_INTERMEDIATE,
        TYRE_GRAVEL_WET,
        TYRE_SNOW,
        NUM_TYRE_TYPES,
        TYRE_TYPE_FORCE_DWORD = 0x7fffffff,
    };

    enum ERBRWeatherType {
        GOOD_WEATHER,
        RANDOM_WEATHER,
        BAD_WEATHER,
    };

    /////////////////////////////////////////////////
    // Game flow control gizmos
    /////////////////////////////////////////////////

    /**
     * @param <iMap>		The map to load, see maplist for map id's
     * @param <iCar>		Which car to use
     * @param <eWeather>   Weather type
     * @param <eTyre>		Tyre type to use
     * @param <ptxtCarSetupFileName> The carsetup file to use (use filepath from gameroot, i.e SavedGames\\5slot0_gravelsetup.lsp ), use null for default setup
     *
     * @return true if game was started
     *
     **/
    virtual bool StartGame(int iMap, int iCar, const ERBRWeatherType eWeather, const ERBRTyreTypes eTyre, const char* ptxtCarSetupFileName) = 0;

    // Draw an ingame message
    /**
     * @param <ptxtMessage> Message to display
     * @param <fTimeToDisplay> Time to display the message (0.0f displays it until the next call)
     * @param <x, y> Display position
     *
     **/
    virtual void WriteGameMessage(const char* ptxtMessage, float fTimeToDisplay, float x, float y) = 0;

    /////////////////////////////////////////////////
    // Menu Drawing functions
    /////////////////////////////////////////////////

    virtual void WriteText(float x, float y, const char* ptxtText) = 0;
    // Draw a 2D Gfx box, see Genbox.h for box defines
    virtual void DrawBox(unsigned int iBox, float x, float y) = 0;
    // Draw a flatcolored box
    virtual void DrawFlatBox(float x, float y, float w, float h) = 0;
    // Draw a blackout (the thing behind the menus), coordinates specify the "window"
    virtual void DrawBlackOut(float x, float y, float w, float h) = 0;
    // Draw the red selection bar
    virtual void DrawSelection(float x, float y, float w, float h = 21.0f) = 0;

    virtual void DrawCarIcon(float x, float y, int iCar) = 0;

#define GFX_DRAW_CENTER_X 0x00000001
#define GFX_DRAW_CENTER_Y 0x00000002
#define GFX_DRAW_ALIGN_RIGHT 0x00000004
#define GFX_DRAW_ALIGN_BOTTOM 0x00000008
#define GFX_DRAW_FLIP_X 0x00000010
#define GFX_DRAW_FLIP_Y 0x00000020
#define GFX_DRAW_TEXT_SHADOW 0x00000040

    // Set draw mode for centering, alignment etc... Don't forget to reset it afterwards
    virtual void SetDrawMode(unsigned int bfDrawMode) = 0;
    virtual void ReSetDrawMode(unsigned int bfDrawMode) = 0;

    enum EFonts {
        FONT_SMALL,
        FONT_BIG,
        FONT_DEBUG,
        FONT_HEADING, // Only uppercase
    };

    // Set font for WriteText
    virtual void SetFont(EFonts eFont) = 0;

    enum EMenuColors {
        MENU_BKGROUND, // Background color
        MENU_SELECTION, // Selection color
        MENU_ICON, // icon color
        MENU_TEXT, // text color
        MENU_HEADING, // heading color
    };

    // Set predefined color
    virtual void SetMenuColor(EMenuColors eColor) = 0;
    // Set arbitrary color
    virtual void SetColor(float r, float g, float b, float a) = 0;

    /////////////////////////////////////////////////
    // Misc functions
    /////////////////////////////////////////////////

    enum EGameLanguage {
        L_ENGLISH,
        L_FRENCH,
        L_GERMAN,
        L_SPANISH,
        L_ITALIAN,
        L_CZECH,
        L_POLISH,
        L_NUM_LANGUAGES
    };

    // Return the language the game uses
    virtual const EGameLanguage GetLanguage(void) = 0;
};

class IPlugin {
public:
    virtual ~IPlugin() { }
    virtual const char* GetName() = 0;
    virtual void DrawFrontEndPage() = 0;
    virtual void DrawResultsUI() = 0;
    virtual void HandleFrontEndEvents(char txtKeyboard, bool bUp, bool bDown, bool bLeft, bool bRight, bool bSelect) = 0;
    virtual void TickFrontEndPage(float fTimeDelta) = 0;
    virtual void StageStarted(int iMap, const char* ptxtPlayerName, bool bWasFalseStart) = 0;
    virtual void HandleResults(float fCheckPoint1, float fCheckPoint2, float fFinishTime, const char* ptxtPlayerName) = 0;
    virtual void CheckPoint(float fCheckPointTime, int iCheckPointID, const char* ptxtPlayerName) = 0;
};
