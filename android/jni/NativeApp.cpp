// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

// NativeApp implementation for platforms that will use that framework, like:
// Android, Linux, MacOSX.
//
// Native is a cross platform framework. It's not very mature and mostly
// just built according to the needs of my own apps.
//
// Windows has its own code that bypasses the framework entirely.

#include "base/logging.h"
#include "base/NativeApp.h"
#include "file/vfs.h"
#include "file/zip_read.h"
#include "gfx/gl_lost_manager.h"
#include "gfx/texture.h"
#include "input/input_state.h"
#include "math/lin/matrix4x4.h"
#include "ui/screen.h"
#include "ui/ui.h"

#include "FileUtil.h"
#include "LogManager.h"
#include "../../Core/PSPMixer.h"
#include "../../Core/CPU.h"
#include "../../Core/Config.h"
#include "../../Core/Host.h"
#include "../../Common/MemArena.h"

#include "ui_atlas.h"
#include "EmuScreen.h"
#include "MenuScreens.h"
#include "UIShader.h"

Texture *uiTexture;

ScreenManager screenManager;
std::string config_filename;

class AndroidLogger : public LogListener
{
public:
  void Log(LogTypes::LOG_LEVELS level, const char *msg)
  {
    switch (level)
    {
    case LogTypes::LDEBUG:
    case LogTypes::LINFO:
      ILOG("%s", msg);
      break;
    case LogTypes::LERROR:
      ELOG("%s", msg);
      break;
    case LogTypes::LWARNING:
      WLOG("%s", msg);
      break;
    case LogTypes::LNOTICE:
    default:
      ILOG("%s", msg);
      break;
    }
  }
};


// TODO: Get rid of this junk
class NativeHost : public Host
{
public:
	NativeHost() {
		// hasRendered = false;
	}

  virtual void UpdateUI() {}

  virtual void UpdateMemView() {}
  virtual void UpdateDisassembly() {}

  virtual void SetDebugMode(bool mode) { }

  virtual void InitGL() {}
  virtual void BeginFrame() {}
  virtual void EndFrame() {}
  virtual void ShutdownGL() {}

  virtual void InitSound(PMixer *mixer);
  virtual void UpdateSound() {};
  virtual void ShutdownSound();

  // this is sent from EMU thread! Make sure that Host handles it properly!
  virtual void BootDone() {} 
  virtual void PrepareShutdown() {}

  virtual bool IsDebuggingEnabled() {return false;}
  virtual bool AttemptLoadSymbolMap() {return false;}
  virtual void ResetSymbolMap() {}
  virtual void AddSymbol(std::string name, u32 addr, u32 size, int type=0) {}
};

// globals
static PMixer *g_mixer = 0;
static AndroidLogger *logger = 0;

std::string boot_filename = "";

void NativeHost::InitSound(PMixer *mixer)
{
	g_mixer = mixer;
}

void NativeHost::ShutdownSound()
{
	g_mixer = 0;
}

void NativeMix(short *audio, int num_samples)
{
  if (g_mixer)
  {
    g_mixer->Mix(audio, num_samples/2);
  }
	else
	{
		//memset(audio, 0, numSamples * 2);
	}
}

void NativeGetAppInfo(std::string *app_dir_name, std::string *app_nice_name, bool *landscape)
{
	*app_nice_name = "PPSSPP";
	*app_dir_name = "ppsspp";
	*landscape = true;
}

void NativeInit(int argc, const char *argv[], const char *savegame_directory, const char *external_directory, const char *installID)
{
  std::string user_data_path = savegame_directory;

  // We want this to be FIRST.
  VFSRegister("", new DirectoryAssetReader("assets/"));
  VFSRegister("", new DirectoryAssetReader(user_data_path.c_str()));

  host = new NativeHost();

  logger = new AndroidLogger();

  LogManager::Init();
  LogManager *logman = LogManager::GetInstance();
  ILOG("Logman: %p", logman);

	if (argc > 1) 
	{
		boot_filename = argv[1];

		if (!File::Exists(boot_filename))
		{
			fprintf(stdout, "File not found: %s\n", boot_filename.c_str());
			exit(1);
		}
	}

	config_filename = user_data_path + "/config.ini";

	g_Config.Load(config_filename.c_str());

	if (g_Config.currentDirectory == "") {
#ifdef ANDROID
		g_Config.currentDirectory = external_directory;
#else
		g_Config.currentDirectory = getenv("HOME");
#endif
	}

  for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
  {
    LogTypes::LOG_TYPE type = (LogTypes::LOG_TYPE)i;
    logman->SetEnable(type, true);
    logman->SetLogLevel(type, LogTypes::LDEBUG);
#ifdef ANDROID
    logman->AddListener(type, logger);
#endif
  }
  logman->SetLogLevel(LogTypes::G3D, LogTypes::LERROR);
  INFO_LOG(BOOT, "Logger inited.");
}

void NativeInitGraphics()
{
	INFO_LOG(BOOT, "NativeInitGraphics - should only be called once!");
	gl_lost_manager_init();
	ui_draw2d.SetAtlas(&ui_atlas);

	screenManager.switchScreen(new LogoScreen(boot_filename));
	// screenManager.switchScreen(new FileSelectScreen());

	UIShader_Init();

	UITheme theme = {0};
	theme.uiFont = UBUNTU24;
	theme.uiFontSmall = UBUNTU24;
	theme.uiFontSmaller = UBUNTU24;
	theme.buttonImage = I_BUTTON;
	theme.buttonSelected = I_BUTTON_SELECTED;
	theme.checkOn = I_CROSS;
	theme.checkOff = I_SQUARE;

	UIInit(&ui_atlas, theme);

	uiTexture = new Texture();
	if (!uiTexture->Load("ui_atlas.zim"))
  {
    ELOG("Failed to load texture");
  }
  uiTexture->Bind(0);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void NativeRender()
{
	glViewport(0, 0, pixel_xres, pixel_yres);
  Matrix4x4 ortho;
	ortho.setOrtho(0.0f, dp_xres, dp_yres, 0.0f, -1.0f, 1.0f);
	glsl_bind(UIShader_Get());
	glUniformMatrix4fv(UIShader_Get()->u_worldviewproj, 1, GL_FALSE, ortho.getReadPtr());
	
	screenManager.render();
}

void NativeUpdate(InputState &input)
{
	UIUpdateMouse(0, input.pointer_x[0], input.pointer_y[0], input.pointer_down[0]);
	screenManager.update(input);
}

void NativeDeviceLost()
{
	screenManager.deviceLost();
	gl_lost();
	// Should dirty EVERYTHING
}

bool NativeIsAtTopLevel()
{
  // TODO
  return false;
}

void NativeTouch(int finger, float x, float y, double time, TouchEvent event)
{
  switch (event) {
  case TOUCH_DOWN:
    break;
  case TOUCH_MOVE:
    break;
  case TOUCH_UP:
    break;
  }
}

void NativeShutdownGraphics()
{
	delete uiTexture;
	uiTexture = NULL;

	screenManager.shutdown();

	UIShader_Shutdown();

  gl_lost_manager_shutdown();
}

void NativeShutdown()
{
  delete host;
  host = 0;
  LogManager::Shutdown();
	g_Config.Save();
	// This means that the activity has been completely destroyed. PPSSPP does not
	// boot up correctly with "dirty" global variables currently, so we hack around that
	// by simply exiting.
	exit(0);
}
