#include "ui_theme.h"
#include "context.h"
#include "utils.h"

namespace rf {
	namespace ui {

		ui_theme Theme;
		ui_theme DefaultTheme;

		const char *DefaultThemeJSON = 
		"{															\n"
			"\"Red\" : [1, 0, 0, 1],								\n"
			"\"Green\" : [0, 1, 0, 1],								\n"
			"\"Blue\" : [0, 0, 1, 1],								\n"
			"\"Black\" : [0, 0, 0, 1],								\n"
			"\"White\" : [1, 1, 1, 1],								\n"
			"														\n"
			"\"ConsoleBG\" : [0, 0, 0, 0.7],						\n"
			"\"PanelBG\" : [0, 0, 0, 0.5],							\n"
			"\"PanelFG\" : [0.8, 0.8, 0.8, 1],						\n"
			"\"TitlebarBG\" : [1, 1, 1, 0.1],						\n"
			"\"BorderBG\" : [1, 1, 1, 0.20],						\n"
			"\"ConsoleFG\" : [1, 1, 1, 0.9],						\n"
			"\"SliderBG\" : [1, 1, 1, 0.2],							\n"
			"\"SliderFG\" : [0, 0, 0, 0.6],							\n"
			"\"ProgressbarBG\" : [0, 0, 0, 0.2],					\n"
			"\"ProgressbarFG\" : [1, 1, 1, 0.1],					\n"
			"														\n"
			"\"ButtonBG\" : [1, 1, 1, 0.1],							\n"
			"\"ButtonPressedBG\" : [1, 1, 1, 0.05],					\n"
			"														\n"
			"\"DebugFG\" : [1, 0, 0, 1],							\n"
			"														\n"
			"\"ConsoleFont\" : [\"data/Roboto-Regular.ttf\", 13],	\n"
			"\"DefaultFont\" : [\"data/Roboto-Regular.ttf\", 13],	\n"
			"\"AwesomeFont\" : [\"data/fontawesome.ttf\", 32]		\n"
		"}";

col4f const &GetColor(theme_color Col)
{
    switch(Col)
    {
        case COLOR_RED : return Theme.Red;
        case COLOR_GREEN : return Theme.Green;
        case COLOR_BLUE : return Theme.Blue;
        case COLOR_WHITE : return Theme.White;
        case COLOR_BLACK : return Theme.Black;
        case COLOR_PANELBG : return Theme.PanelBG;
        case COLOR_PANELFG : return Theme.PanelFG;
        case COLOR_TITLEBARBG : return Theme.TitlebarBG;
        case COLOR_BORDERBG : return Theme.BorderBG;
        case COLOR_CONSOLEBG : return Theme.ConsoleBG;
        case COLOR_CONSOLEFG : return Theme.ConsoleFG;
        case COLOR_DEBUGFG : return Theme.DebugFG;
        case COLOR_SLIDERBG : return Theme.SliderBG;
        case COLOR_SLIDERFG : return Theme.SliderFG;
        case COLOR_BUTTONBG : return Theme.ButtonBG;
        case COLOR_BUTTONPRESSEDBG : return Theme.ButtonPressedBG;
        case COLOR_PROGRESSBARBG : return Theme.ProgressbarBG;
        case COLOR_PROGRESSBARFG : return Theme.ProgressbarFG;
        default : return Theme.White;
    }
}

font *GetFont(theme_font Font)
{
    switch(Font)
    {
        case FONT_DEFAULT : return Theme.DefaultFont;
        case FONT_CONSOLE : return Theme.ConsoleFont;
        case FONT_AWESOME : return Theme.AwesomeFont;
        default : return Theme.DefaultFont;
    }
}

int32 GetFontLineGap(theme_font Font)
{
    switch(Font)
    {
        case FONT_DEFAULT : return Theme.DefaultFont->LineGap;
        case FONT_CONSOLE : return Theme.ConsoleFont->LineGap;
        case FONT_AWESOME : return Theme.AwesomeFont->LineGap;
        default : return Theme.DefaultFont->LineGap;
    }
}

static font *ParseConfigFont(cJSON *root, context *Context, char const *Name, int c0, int cn)
{
    cJSON *FontInfo = cJSON_GetObjectItem(root, Name);
    if(FontInfo && cJSON_GetArraySize(FontInfo) == 2)
    {
        path FontPath;
        strncpy(FontPath, cJSON_GetArrayItem(FontInfo, 0)->valuestring, MAX_PATH);
        int FontSize = cJSON_GetArrayItem(FontInfo, 1)->valueint;
        return ResourceLoadFont(Context, FontPath, FontSize, c0, cn);
    }
    else
    {
        printf("Error loading UI Theme Font %s, loading default DroidSansMono instead.\n", Name);
        return ResourceLoadFont(Context, "data/DroidSansMonoSlashed.ttf", 13, 32, 127);
    }
}

static void ParseUIConfigRoot(ui_theme *DstTheme, cJSON *root, context *Context)
{
    DstTheme->Red = JSON_Get(root, "Red", DefaultTheme.Red);
    DstTheme->Green = JSON_Get(root, "Green", DefaultTheme.Green);
    DstTheme->Blue = JSON_Get(root, "Blue", DefaultTheme.Blue);
    DstTheme->Black = JSON_Get(root, "Black", DefaultTheme.Black);
    DstTheme->White = JSON_Get(root, "White", DefaultTheme.White);

    DstTheme->PanelBG = JSON_Get(root, "PanelBG", DefaultTheme.PanelBG);
    DstTheme->PanelFG = JSON_Get(root, "PanelFG", DefaultTheme.PanelFG);
    DstTheme->TitlebarBG = JSON_Get(root, "TitlebarBG", DefaultTheme.TitlebarBG);
    DstTheme->BorderBG = JSON_Get(root, "BorderBG", DefaultTheme.BorderBG);
    DstTheme->ConsoleBG = JSON_Get(root, "ConsoleBG", DefaultTheme.ConsoleBG);
    DstTheme->ConsoleFG = JSON_Get(root, "ConsoleFG", DefaultTheme.ConsoleFG);
    DstTheme->DebugFG = JSON_Get(root, "DebugFG", DefaultTheme.DebugFG);
    DstTheme->SliderBG = JSON_Get(root, "SliderBG", DefaultTheme.SliderBG);
    DstTheme->SliderFG = JSON_Get(root, "SliderFG", DefaultTheme.SliderFG);
    DstTheme->ButtonBG = JSON_Get(root, "ButtonBG", DefaultTheme.ButtonBG);
    DstTheme->ButtonPressedBG = JSON_Get(root, "ButtonPressedBG", DefaultTheme.ButtonPressedBG);
    DstTheme->ProgressbarBG = JSON_Get(root, "ProgressbarBG", DefaultTheme.ProgressbarBG);
    DstTheme->ProgressbarFG = JSON_Get(root, "ProgressbarFG", DefaultTheme.ProgressbarFG);

    DstTheme->DefaultFont = ParseConfigFont(root, Context, "DefaultFont", 32, 127);
    DstTheme->ConsoleFont = ParseConfigFont(root, Context, "ConsoleFont", 32, 127);
    DstTheme->AwesomeFont = ParseConfigFont(root, Context, "AwesomeFont", ICON_MIN_FA, 1+ICON_MAX_FA);
}

void ParseDefaultUIConfig(context *Context)
{
    path DefaultConfigPath;
    ConcatStrings(DefaultConfigPath, ctx::GetExePath(Context), "default_ui_config.json");

	void *Content = nullptr;

    // If the default config doesnt exist, create it
    if(!DiskFileExists(DefaultConfigPath))
    {
        //printf("Fatal Error : Default UI Config file bin/default_ui_config.json doesn't exist.\n");
        //exit(1);
		Content = (void*)DefaultThemeJSON;
    }
	else
	{
		Content = ReadFileContents(Context, DefaultConfigPath, 0);
		if (!Content)
		{
			Assert(false); // should never happen because of the file check
		}
	}

	cJSON *root = cJSON_Parse((char*)Content);
	if (root)
	{
		ParseUIConfigRoot(&DefaultTheme, root, Context);
	}
	else
	{
		printf("Fatal Error parsing UI Config File (%s) as JSON.\n", DefaultConfigPath);
		exit(1);
	}
}

void ParseUIConfig(context *Context, path const ConfigPath)
{
    void *Content = ReadFileContents(Context, ConfigPath, 0);
    if(Content)
    {
        cJSON *root = cJSON_Parse((char*)Content);
        if(root)
        {
            ParseUIConfigRoot(&Theme, root, Context);
        }
        else
        {
            printf("Error parsing UI Config File (%s) as JSON. Using Default Theme.\n", ConfigPath);
            Theme = DefaultTheme;
        }
    }
    else
    {
        printf("Generating UI theme from Default Theme...\n");

        path DefaultConfigPath;
        ConcatStrings(DefaultConfigPath, ctx::GetExePath(Context), "default_ui_config.json");
        path PersonalConfigPath;
        ConcatStrings(PersonalConfigPath, ctx::GetExePath(Context), "ui_config.json");
        DiskFileCopy(PersonalConfigPath, DefaultConfigPath);

        Theme = DefaultTheme;
    }
}

}
}
