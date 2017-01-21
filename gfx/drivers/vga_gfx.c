/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2016 - Brad Parker
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <retro_miscellaneous.h>
#include <dpmi.h>
#include <pc.h>

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../common/vga_common.h"

#include "../font_driver.h"

#include "../../driver.h"
#include "../../verbosity.h"

static unsigned char *vga_menu_frame = NULL;
static unsigned vga_menu_width       = 0;
static unsigned vga_menu_height      = 0;
static unsigned vga_menu_pitch       = 0;
static unsigned vga_menu_bits        = 0;
static unsigned vga_video_width      = 0;
static unsigned vga_video_height     = 0;
static unsigned vga_video_pitch      = 0;
static unsigned vga_video_bits       = 0;
static bool vga_rgb32                = false;

static float lerp(float x, float y, float a, float b, float d)
{
   return a + (b - a) * ((d - x) / (y - x));
}

static void set_mode_13h()
{
   __dpmi_regs r;

   r.x.ax = 0x13;
   __dpmi_int(0x10, &r);
}

static void return_to_text_mode()
{
   __dpmi_regs r;

   r.x.ax = 3;
   __dpmi_int(0x10, &r);
}

static void vga_vsync()
{
   /* wait until any previous retrace has ended */
   do
   {
   }
   while (inportb(0x3DA) & 8);

   /* wait until a new retrace has just begun */
   do
   {
   }
   while (!(inportb(0x3DA) & 8));
}

static void vga_gfx_create(void)
{
   set_mode_13h();
}

static void *vga_gfx_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   vga_t *vga        = (vga_t*)calloc(1, sizeof(*vga));

   *input              = NULL;
   *input_data         = NULL;

   vga_video_width    = video->width;
   vga_video_height   = video->height;
   vga_rgb32          = video->rgb32;

   if (video->rgb32)
      vga_video_pitch = video->width * 4;
   else
      vga_video_pitch = video->width * 2;

   vga_gfx_create();

   if (video->font_enable)
      font_driver_init_osd(NULL, false, FONT_DRIVER_RENDER_VGA);

   return vga;
}

static bool vga_gfx_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   size_t len = 0;
   void *buffer = NULL;
   unsigned width, height, bits;
   const void *frame_to_copy = frame;
   bool draw = true;

   (void)data;
   (void)frame;
   (void)frame_width;
   (void)frame_height;
   (void)pitch;
   (void)msg;

   if (!frame || !frame_width || !frame_height)
      return true;

#ifdef HAVE_MENU
   menu_driver_frame(video_info);
#endif

   if (  vga_video_width  != frame_width   ||
         vga_video_height != frame_height  ||
         vga_video_pitch  != pitch)
   {
      if (frame_width > 4 && frame_height > 4)
      {
         vga_video_width = frame_width;
         vga_video_height = frame_height;
         vga_video_pitch = pitch;
      }
   }

   if (vga_menu_frame && menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL))
   {
      frame_to_copy = vga_menu_frame;
      width         = vga_menu_width;
      height        = vga_menu_height;
      pitch         = vga_menu_pitch;
      bits          = vga_menu_bits;
   }
   else
   {
      width         = vga_video_width;
      height        = vga_video_height;
      pitch         = vga_video_pitch;

      if (frame_width == 4 && frame_height == 4 && (frame_width < width && frame_height < height))
         draw = false;

      if (menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL))
         draw = false;
   }

   if (draw)
   {
      vga_vsync();
      dosmemput(frame_to_copy, MIN(320,width)*MIN(200,height), 0xA0000);
   }

   if (msg)
      font_driver_render_msg(video_info, NULL, msg, NULL);

   video_context_driver_update_window_title(video_info);

   return true;
}

static void vga_gfx_set_nonblock_state(void *data, bool toggle)
{
   (void)data;
   (void)toggle;
}

static bool vga_gfx_alive(void *data)
{
   (void)data;
   video_driver_set_size(&vga_video_width, &vga_video_height);
   return true;
}

static bool vga_gfx_focus(void *data)
{
   (void)data;
   return true;
}

static bool vga_gfx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool vga_gfx_has_windowed(void *data)
{
   (void)data;
   return true;
}

static void vga_gfx_free(void *data)
{
   (void)data;

   if (vga_menu_frame)
   {
      free(vga_menu_frame);
      vga_menu_frame = NULL;
   }

   return_to_text_mode();
}

static bool vga_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void vga_gfx_set_rotation(void *data,
      unsigned rotation)
{
   (void)data;
   (void)rotation;
}

static void vga_gfx_viewport_info(void *data,
      struct video_viewport *vp)
{
   (void)data;
   (void)vp;
}

static bool vga_gfx_read_viewport(void *data, uint8_t *buffer)
{
   (void)data;
   (void)buffer;

   return true;
}

static void vga_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   unsigned pitch = width * 2;

   if (rgb32)
      pitch = width * 4;

   if (vga_menu_frame)
   {
      free(vga_menu_frame);
      vga_menu_frame = NULL;
   }

   if ( !vga_menu_frame ||
         vga_menu_width  != width  ||
         vga_menu_height != height ||
         vga_menu_pitch  != pitch)
      if (pitch && height)
         vga_menu_frame = (unsigned char*)malloc(VGA_WIDTH * VGA_HEIGHT);

   if (vga_menu_frame && frame && pitch && height)
   {
      unsigned x, y;

      if (rgb32)
      {
      }
      else
      {
         unsigned short *video_frame = (unsigned short*)frame;

         for(y = 0; y < VGA_HEIGHT; y++)
         {
            for(x = 0; x < VGA_WIDTH; x++)
            {
                vga_menu_frame[VGA_WIDTH * y + x] = lerp(0, 65535, 0, 254, video_frame[width * y + x]);
            }
         }
      }

      vga_menu_width  = width;
      vga_menu_height = height;
      vga_menu_pitch  = pitch;
      vga_menu_bits   = rgb32 ? 32 : 16;
   }
}

static void vga_set_osd_msg(void *data, const char *msg,
      const void *params, void *font)
{
   video_frame_info_t video_info;
   video_driver_build_info(&video_info);
   font_driver_render_msg(&video_info, font, msg, params);
}

static const video_poke_interface_t vga_poke_interface = {
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
#ifdef HAVE_FBO
   NULL,
#else
   NULL,
#endif
   NULL,
   NULL,
   NULL,
#if defined(HAVE_MENU)
   vga_set_texture_frame,
   NULL,
   vga_set_osd_msg,
   NULL,
#else
   NULL,
   NULL,
   NULL,
   NULL,
#endif

   NULL,
#ifdef HAVE_MENU
   NULL,
#endif
};

static void vga_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &vga_poke_interface;
}

void vga_gfx_set_viewport(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate)
{
}

video_driver_t video_vga = {
   vga_gfx_init,
   vga_gfx_frame,
   vga_gfx_set_nonblock_state,
   vga_gfx_alive,
   vga_gfx_focus,
   vga_gfx_suppress_screensaver,
   vga_gfx_has_windowed,
   vga_gfx_set_shader,
   vga_gfx_free,
   "vga",
   vga_gfx_set_viewport,
   vga_gfx_set_rotation,
   vga_gfx_viewport_info,
   vga_gfx_read_viewport,
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
  vga_gfx_get_poke_interface,
};