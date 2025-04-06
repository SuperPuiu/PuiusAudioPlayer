#include "microui.h"
#include "gui.h"
#include "render.h"
#include "pfd.h"
#include "audio.h"

#ifndef WINDOWS
#include <dirent.h>
#include <sys/stat.h>
#include <SDL3_mixer/SDL_mixer.h>
#else
#include <SDL3/SDL_mixer.h>
#endif

#include <string.h>

int LoopStatus = LOOP_NONE;
static int PlaylistWidths[PAP_MAX_AUDIO];
static int TitleOpt = MU_OPT_NORESIZE | MU_OPT_NOSCROLL | MU_OPT_NOCLOSE | MU_OPT_NOINTERACT | MU_OPT_NOFRAME;
static int BelowOpt = MU_OPT_NORESIZE | MU_OPT_NOSCROLL | MU_OPT_NOCLOSE | MU_OPT_NOTITLE | MU_OPT_NOFRAME;
static int PlaylistOpt = MU_OPT_NOCLOSE | MU_OPT_NOTITLE;
static int ExtraOpt = MU_OPT_NOCLOSE | MU_OPT_NOTITLE | MU_OPT_NOFRAME | MU_OPT_NOSCROLL;
static int SelectedAudio;

float l_AudioPosition;
static float AudioFloat = MIX_MAX_VOLUME;

static mu_Rect PAP_Title;
static mu_Rect PAP_Below;
static mu_Rect PAP_Extra;
static mu_Rect PAP_Playlist;

static const char *InteractButtonText = "Pause";
static const char *LoopButtonText = "No loop";

int TextWidth(mu_Font font, const char *text, int len) {
  if (len == -1) { len = strlen(text); }
  return r_get_text_width(text, len);
}

int TextHeight(mu_Font font) {
  return r_get_text_height();
}

int PAP_AudioButton(mu_Context *Context, const char *Name, int AudioID) {
  mu_Id ButtonID = mu_get_id(Context, Name, sizeof(Name));
  mu_Rect Rect = mu_layout_next(Context);
  
  mu_update_control(Context, ButtonID, Rect, 0);

  int Result = 0;
  
  if (Context->mouse_pressed == MU_MOUSE_LEFT && Context->focus == ButtonID) {
    PlayAudio(Audio[AudioID].Path);
    Result |= MU_RES_CHANGE;
  } else if (Context->mouse_pressed == MU_MOUSE_RIGHT && Context->focus == ButtonID && AudioID != AudioCurrentIndex) {
    mu_open_popup(Context, "Menu");
    SelectedAudio = AudioID;
  }
  
  mu_draw_control_frame(Context, ButtonID, Rect, AudioCurrentIndex != AudioID ? MU_COLOR_BUTTON : MU_COLOR_BASE, 0); /* Main button frame */
  mu_draw_control_text(Context, Name, Rect, MU_COLOR_TEXT, 0);
  // mu_draw_control_text();

  return Result;
}

int PAP_Slider(mu_Context *Context, mu_Real *Value, int Low, int High) {
  int Result = 0;
  mu_Rect *l_Rect;

  mu_push_id(Context, &Value, sizeof(Value));

  /* Get the next rect, save it and push it back */
  mu_Rect Rect = mu_layout_next(Context);
  l_Rect = &Rect;
  mu_layout_set_next(Context, Rect, 0);

  Result = mu_slider_ex(Context, Value, Low, High, 0, "%.2f", MU_OPT_ALIGNCENTER);

  if (Context->scroll_delta.y != 0 && mu_mouse_over(Context, *l_Rect)) {
    *Value += Context->scroll_delta.y / (Context->scroll_delta.y / 2) * (Context->scroll_delta.y > 0 ? -1 : 1);
    Result |= MU_RES_CHANGE;
  }

  mu_pop_id(Context);
  return Result;
}

void InitializeGUI() {
  for (int i = 0; i < PAP_MAX_AUDIO; i++)
    PlaylistWidths[i] = PLAYLIST_WIDTH - 25;
  
  PAP_Title = mu_rect(0, 0, WINDOW_WIDTH, 20);
  PAP_Below = mu_rect(0, WINDOW_HEIGHT - BELOW_HEIGHT, WINDOW_WIDTH, BELOW_HEIGHT);
  PAP_Playlist = mu_rect(WINDOW_WIDTH / 2 - PLAYLIST_WIDTH / 2, WINDOW_HEIGHT / 2 - PLAYLIST_HEIGHT / 2, PLAYLIST_WIDTH, PLAYLIST_HEIGHT);
  PAP_Extra = mu_rect(0, WINDOW_HEIGHT - BELOW_HEIGHT - EXTRA_HEIGHT, WINDOW_WIDTH, EXTRA_HEIGHT);
}

void MainWindow(mu_Context *Context) {
  mu_Color OldColor;

  /* Title */
  if (mu_begin_window_ex(Context, "Puius Audio Player", PAP_Title, TitleOpt)) {
    mu_end_window(Context);
  }

  /* Below */
  if (mu_begin_window_ex(Context, "BELOW", PAP_Below, BelowOpt)) {
    mu_layout_row(Context, 4, (int[]){100, 100, 300, 100}, 25);

    if (mu_button(Context, "Choose directory")) {
      const char *Path = OpenDialogue(PFD_DIRECTORY);

      if (Path) {
        #ifndef WINDOWS
        DIR *Directory;
        struct dirent *Entry; // I guess the name is right

        if ((Directory = opendir(Path)) != NULL) {
          size_t PathLen = strlen(Path);
          char FullPath[PATH_MAX];
          
          memcpy(FullPath, Path, PathLen);

          while ((Entry = readdir(Directory)) != NULL) {
            struct stat Stats;
            
            /* Not interested into those directories. */
            if (strcmp(Entry->d_name, ".") == 0 || strcmp(Entry->d_name, "..") == 0)
              continue;
            
            /* Compute absolute path */
            FullPath[PathLen] = '\0';
            strcat(FullPath, "/");
            strcat(FullPath, Entry->d_name);

            if (stat(FullPath, &Stats) == -1) {
              SDL_Log("stat() failed on %s.", Entry->d_name);
              continue;
            }

            if (S_ISREG(Stats.st_mode) != 0)
              AddAudio(FullPath);
          }

          closedir(Directory);
        } else {
          SDL_Log("Directory is NULL.");
        }
        #endif
      }
    }

    if (mu_button(Context, "Choose file")) {
      const char *Path = OpenDialogue(PFD_FILE);

      if (Path)
        AddAudio((char *)Path);
      else
        SDL_Log("Path is NULL.\n");
    }
    
    mu_layout_set_next(Context, mu_rect(WINDOW_WIDTH / 2 - 150, 0, 300, 25), 1);
    if (mu_slider_ex(Context, &l_AudioPosition, 0, AudioDuration, 0, MU_SLIDER_FMT, MU_OPT_ALIGNCENTER)) {
      // Mix_PauseMusic();
      AudioPosition = (double)l_AudioPosition;
      Mix_SetMusicPosition(AudioPosition);
    }

    l_AudioPosition = AudioPosition;
    
    mu_layout_set_next(Context, mu_rect(WINDOW_WIDTH - 110, 0, 100, 25), 1);
    if (PAP_Slider(Context, &AudioFloat, 0, 128)) {
      AudioVolume = (int)AudioFloat;
      Mix_VolumeMusic(AudioVolume);
    }

    mu_end_window(Context);
  }
  
  /* Playlist */
  OldColor = Context->style->colors[MU_COLOR_BORDER];
  Context->style->colors[MU_COLOR_BORDER] = Context->style->colors[MU_COLOR_WINDOWBG];
  
  if (mu_begin_window_ex(Context, "Playlist", PAP_Playlist, PlaylistOpt)) {
    Context->style->colors[MU_COLOR_BORDER] = OldColor;

    for (int i = 0; i < PAP_MAX_AUDIO; i++) {
      if (Audio[i].Path[0] == 0)
        continue;

      mu_layout_row(Context, 0, NULL, 25);
      mu_layout_width(Context, PLAYLIST_WIDTH - 25);

      PAP_AudioButton(Context, Audio[i].Title, i);
    }
    
    mu_Container *Container = mu_get_container(Context, "Menu");
    
    if (mu_begin_popup(Context, "Menu")) {
      if (mu_button(Context, "Remove")) {
        AudioRemove(SelectedAudio);
        Container->open = 0;
      }

      mu_end_popup(Context);
    }
    
    mu_end_window(Context);
  }

  /* Extra */
  if (mu_begin_window_ex(Context, "EXTRA", PAP_Extra, ExtraOpt)) {
    mu_Rect InteractionRect = mu_rect(WINDOW_WIDTH / 2 - 105, 10, 100, 20);
    mu_Rect LoopRect = mu_rect(InteractionRect.x + 105, InteractionRect.y, InteractionRect.w, InteractionRect.h);

    mu_layout_set_next(Context, InteractionRect, 1);
    
    if (mu_button_ex(Context, InteractButtonText, 0, MU_OPT_ALIGNCENTER)) {
      if (Mix_PausedMusic()) {
        Mix_ResumeMusic();
        InteractButtonText = "Pause";
      } else {
        Mix_PauseMusic();
        InteractButtonText = "Resume";
      }
    }

    mu_layout_set_next(Context, LoopRect, 1);
    
    if (mu_button_ex(Context, LoopButtonText, 0, MU_OPT_ALIGNCENTER)) {
      if (LoopStatus == LOOP_NONE) {
        LoopButtonText = "Looping song";
        LoopStatus = LOOP_SONG;
      } else if (LoopStatus == LOOP_SONG) {
        LoopButtonText = "Looping all";
        LoopStatus = LOOP_ALL;
      } else if (LoopStatus == LOOP_ALL) {
        LoopButtonText = "No loop";
        LoopStatus = LOOP_NONE;
      }
    }

    mu_end_window(Context);
  }
}

void ProcessContextFrame(mu_Context *Context) {
  mu_begin(Context);
  MainWindow(Context);
  mu_end(Context);
}
