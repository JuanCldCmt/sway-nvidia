#include <assert.h>
#include "config.h"
#include "log.h"
#include "swaybar/image.h"

#if HAVE_GDK_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif

#if HAVE_GDK_PIXBUF
static cairo_surface_t* gdk_cairo_image_surface_create_from_pixbuf(
		const GdkPixbuf *gdkbuf) {
	int chan = gdk_pixbuf_get_n_channels(gdkbuf);
	if (chan < 3) {
		return NULL;
	}

	const guint8* gdkpix = gdk_pixbuf_read_pixels(gdkbuf);
	if (!gdkpix) {
		return NULL;
	}
	gint w = gdk_pixbuf_get_width(gdkbuf);
	gint h = gdk_pixbuf_get_height(gdkbuf);
	int stride = gdk_pixbuf_get_rowstride(gdkbuf);

	cairo_format_t fmt = (chan == 3) ? CAIRO_FORMAT_RGB24 : CAIRO_FORMAT_ARGB32;
	cairo_surface_t * cs = cairo_image_surface_create (fmt, w, h);
	cairo_surface_flush (cs);
	if ( !cs || cairo_surface_status(cs) != CAIRO_STATUS_SUCCESS) {
		return NULL;
	}

	int cstride = cairo_image_surface_get_stride(cs);
	unsigned char * cpix = cairo_image_surface_get_data(cs);

	if (chan == 3) {
		int i;
		for (i = h; i; --i) {
			const guint8 *gp = gdkpix;
			unsigned char *cp = cpix;
			const guint8* end = gp + 3*w;
			while (gp < end) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
				cp[0] = gp[2];
				cp[1] = gp[1];
				cp[2] = gp[0];
#else
				cp[1] = gp[0];
				cp[2] = gp[1];
				cp[3] = gp[2];
#endif
				gp += 3;
				cp += 4;
			}
			gdkpix += stride;
			cpix += cstride;
		}
	} else {
		/* premul-color = alpha/255 * color/255 * 255 = (alpha*color)/255
		 * (z/255) = z/256 * 256/255     = z/256 (1 + 1/255)
		 *         = z/256 + (z/256)/255 = (z + z/255)/256
		 *         # recurse once
		 *         = (z + (z + z/255)/256)/256
		 *         = (z + z/256 + z/256/255) / 256
		 *         # only use 16bit uint operations, loose some precision,
		 *         # result is floored.
		 *       ->  (z + z>>8)>>8
		 *         # add 0x80/255 = 0.5 to convert floor to round
		 *       =>  (z+0x80 + (z+0x80)>>8 ) >> 8
		 * ------
		 * tested as equal to lround(z/255.0) for uint z in [0..0xfe02]
		 */
#define PREMUL_ALPHA(x,a,b,z) \
		G_STMT_START { z = a * b + 0x80; x = (z + (z >> 8)) >> 8; } \
		G_STMT_END
		int i;
		for (i = h; i; --i) {
			const guint8 *gp = gdkpix;
			unsigned char *cp = cpix;
			const guint8* end = gp + 4*w;
			guint z1, z2, z3;
			while (gp < end) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
				PREMUL_ALPHA(cp[0], gp[2], gp[3], z1);
				PREMUL_ALPHA(cp[1], gp[1], gp[3], z2);
				PREMUL_ALPHA(cp[2], gp[0], gp[3], z3);
				cp[3] = gp[3];
#else
				PREMUL_ALPHA(cp[1], gp[0], gp[3], z1);
				PREMUL_ALPHA(cp[2], gp[1], gp[3], z2);
				PREMUL_ALPHA(cp[3], gp[2], gp[3], z3);
				cp[0] = gp[3];
#endif
				gp += 4;
				cp += 4;
			}
			gdkpix += stride;
			cpix += cstride;
		}
#undef PREMUL_ALPHA
	}
	cairo_surface_mark_dirty(cs);
	return cs;
}
#endif // HAVE_GDK_PIXBUF

cairo_surface_t *load_image(const char *path) {
	cairo_surface_t *image;
#if HAVE_GDK_PIXBUF
	GError *err = NULL;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(path, &err);
	if (!pixbuf) {
		sway_log(SWAY_ERROR, "Failed to load background image (%s).",
				err->message);
		return NULL;
	}
	image = gdk_cairo_image_surface_create_from_pixbuf(pixbuf);
	g_object_unref(pixbuf);
#else
	image = cairo_image_surface_create_from_png(path);
#endif // HAVE_GDK_PIXBUF
	if (!image) {
		sway_log(SWAY_ERROR, "Failed to read background image.");
		return NULL;
	}
	if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
		sway_log(SWAY_ERROR, "Failed to read background image: %s."
#if !HAVE_GDK_PIXBUF
				"\nSway was compiled without gdk_pixbuf support, so only"
				"\nPNG images can be loaded. This is the likely cause."
#endif // !HAVE_GDK_PIXBUF
				, cairo_status_to_string(cairo_surface_status(image)));
		return NULL;
	}
	return image;
}
