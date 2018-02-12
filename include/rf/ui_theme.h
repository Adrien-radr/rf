#ifndef RF_UI_THEME_H
#define RF_UI_THEME_H

// This file should only be included in ui.cpp

#include "ui.h"

namespace rf {
namespace ui {

struct ui_theme
{
    col4f Red;
    col4f Green;
    col4f Blue;
    col4f Black;
    col4f White;

    col4f PanelBG;
    col4f PanelFG;
    col4f TitlebarBG;
    col4f BorderBG;
    col4f ConsoleBG;
    col4f ConsoleFG;
    col4f SliderBG;
    col4f SliderFG;
    col4f ButtonBG;
    col4f ButtonPressedBG;
    col4f ProgressbarBG;
    col4f ProgressbarFG;

    col4f DebugFG;

    font  *DefaultFont;
    font  *ConsoleFont;
    font  *AwesomeFont;
};

// Defined in ui_theme.cpp
extern ui_theme Theme;
extern ui_theme DefaultTheme;

void ParseUIConfig(context *Context, path const ConfigPath);
void ParseDefaultUIConfig(context *Context);
}
}
#endif

