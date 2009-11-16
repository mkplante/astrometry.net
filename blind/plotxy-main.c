/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.
  Copyright 2009 Dustin Lang.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <math.h>
#include <string.h>
#include <stdint.h>
#include <sys/param.h>

#include <cairo.h>
#include <cairo-pdf.h>
#ifndef ASTROMETRY_NO_PPM
#include <ppm.h>
#endif

#include "xylist.h"
#include "boilerplate.h"
#include "cairoutils.h"
#include "log.h"
#include "errors.h"

#define OPTIONS "hW:H:n:N:r:s:i:e:x:y:w:S:I:PC:X:Y:b:o:pJ"

static void printHelp(char* progname) {
	boilerplate_help_header(stdout);
	printf("\nUsage: %s [options] > output.png\n"
		   "  -i <input-file>   Input file (xylist)\n"
           "  [-o <output-file>] (default: stdout)\n"
           "  [-I <image>   ]   Input image on which plotting will occur; PPM format.\n"
	       "  [-p]: Input image is PNG format, not PPM.\n"
           "  [-P]              Write PPM output instead of PNG.\n"
		   "  [-J]              Write PDF output.\n"
		   "  [-W <width>   ]   Width of output image (default: data-dependent).\n"
		   "  [-H <height>  ]   Height of output image (default: data-dependent).\n"
		   "  [-x <x-offset>]   X offset: position of the bottom-left pixel.\n"
		   "  [-y <y-offset>]   Y offset: position of the bottom-left pixel.\n"
		   "  [-X <x-column-name>] X column: name of the FITS column.\n"
		   "  [-Y <y-column-name>] Y column: name of the FITS column.\n"
		   "  [-n <first-obj>]  First object to plot (default: 0).\n"
		   "  [-N <num-objs>]   Number of objects to plot (default: all).\n"
		   "  [-r <radius>]     Size of markers to plot (default: 5.0).\n"
		   "  [-w <linewidth>]  Linewidth (default: 1.0).\n"
		   "  [-s <shape>]      Shape of markers (default: circle):",
           progname);
    cairoutils_print_marker_names("\n                 ");
    printf("\n");
    printf("  [-C <color>]      Color to plot in: (default: white)\n");
    cairoutils_print_color_names("\n                 ");
    printf("\n");
    printf("  [-b <color>]      Draw in <color> behind each marker.\n"
           "  [-S <scale-factor>]  Scale xylist entries by this value before plotting.\n"
		   "  [-e <extension>]  FITS extension to read (default 0).\n"
		   "\n");
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *args[]) {
	int argchar;
	char* progname = args[0];
    char* outfn = "-";
    char* infn = NULL;
	char* fname = NULL;
	int W = 0, H = 0;
	int n = 0, N = 0;
	double xoff = 0.0, yoff = 0.0;
	int ext = 1;
	double rad = 5.0;
	double lw = 1.0;
	char* shape = "circle";
	xylist_t* xyls;
	starxy_t* xy;
	int Nxy;
	int i;
	double scale = 1.0;
    bool ppmoutput = FALSE;
    bool pdfoutput = FALSE;

    bool pnginput = FALSE;
    unsigned char* img = NULL;
	cairo_t* cairo;
	cairo_surface_t* target;
    float r=1.0, g=1.0, b=1.0;
    char* xcol = NULL;
    char* ycol = NULL;
    int marker;
    bool background = FALSE;
    float br=0.0, bg=0.0, bb=0.0;

	while ((argchar = getopt(argc, args, OPTIONS)) != -1)
		switch (argchar) {
        case 'C':
            if (cairoutils_parse_color(optarg, &r, &g, &b)) {
                fprintf(stderr, "I didn't understand color \"%s\".\n", optarg);
                exit(-1);
            }
            break;
        case 'b':
            background = TRUE;
            if (cairoutils_parse_color(optarg, &br, &bg, &bb)) {
                fprintf(stderr, "I didn't understand color \"%s\".\n", optarg);
                exit(-1);
            }
            break;
        case 'o':
            outfn = optarg;
            break;
        case 'X':
            xcol = optarg;
            break;
        case 'Y':
            ycol = optarg;
            break;
        case 'P':
            ppmoutput = TRUE;
            break;
		case 'J':
			pdfoutput = TRUE;
			break;
		case 'p':
		  pnginput = TRUE;
            break;
        case 'I':
            infn = optarg;
            break;
		case 'S':
			scale = atof(optarg);
			break;
		case 'i':
			fname = optarg;
			break;
		case 'x':
			xoff = atof(optarg);
			break;
		case 'y':
			yoff = atof(optarg);
			break;
		case 'W':
			W = atoi(optarg);
			break;
		case 'H':
			H = atoi(optarg);
			break;
		case 'n':
			n = atoi(optarg);
			break;
		case 'N':
			N = atoi(optarg);
			break;
		case 'e':
			ext = atoi(optarg);
			break;
		case 'r':
			rad = atof(optarg);
			break;
		case 'w':
			lw = atof(optarg);
			break;
		case 's':
			shape = optarg;
			break;
		case 'h':
			printHelp(progname);
            exit(0);
		case '?':
		default:
			printHelp(progname);
            exit(-1);
		}

	if (optind != argc) {
		printHelp(progname);
		exit(-1);
	}
    if (infn && (W || H)) {
        printf("Error: if you specify an input file, you can't give -W or -H (width or height) arguments.\n\n");
        printHelp(progname);
        exit(-1);
    }
	if (!fname) {
		printHelp(progname);
		exit(-1);
	}

    // log errors to stderr, not stdout.
    errors_log_to(stderr);

	// Open xylist.
	xyls = xylist_open(fname);
	if (!xyls) {
		ERROR("Failed to open xylist from file \"%s\"", fname);
		exit(-1);
	}
    // we don't care about FLUX and BACKGROUND columns.
    xylist_set_include_flux(xyls, FALSE);
    xylist_set_include_background(xyls, FALSE);
    if (xcol)
        xylist_set_xname(xyls, xcol);
    if (ycol)
        xylist_set_yname(xyls, ycol);

	// Find number of entries in xylist.
    xy = xylist_read_field_num(xyls, ext, NULL);
    if (!xy) {
		fprintf(stderr, "Failed to read FITS extension %i from file %s.\n", ext, fname);
		exit(-1);
	}
    Nxy = starxy_n(xy);

	// If N is specified, apply it as a max.
    if (N)
        Nxy = MIN(Nxy, N);

	// Scale xylist entries.
	if (scale != 1.0) {
		for (i=0; i<Nxy; i++) {
			starxy_setx(xy, i, scale * starxy_getx(xy, i));
			starxy_sety(xy, i, scale * starxy_gety(xy, i));
		}
	}

	xylist_close(xyls);

	// if required, scan data for max X,Y
	if (!W) {
		double maxX = 0.0;
		for (i=n; i<Nxy; i++)
            maxX = MAX(maxX, starxy_getx(xy, i));
		W = ceil(maxX + rad - xoff);
	}
	if (!H) {
		double maxY = 0.0;
		for (i=n; i<Nxy; i++)
            maxY = MAX(maxY, starxy_gety(xy, i));
		H = ceil(maxY + rad - yoff);
	}

    if (infn) {
#ifdef ASTROMETRY_NO_PPM
	pnginput = TRUE;
#endif
      if (pnginput) {
	img = cairoutils_read_png(infn, &W, &H);
      }
#ifndef ASTROMETRY_NO_PPM
      else {
	ppm_init(&argc, args);
	img = cairoutils_read_ppm(infn, &W, &H);
      }
#endif // ASTROMETRY_NO_PPM
        if (!img) {
            fprintf(stderr, "Failed to read input image %s.\n", infn);
            exit(-1);
        }
        cairoutils_rgba_to_argb32(img, W, H);
    }

	//fprintf(stderr, "Image size %i x %i.\n", W, H);

	// Allocate cairo surface
	if (pdfoutput) {
		cairo_surface_t* thissurf;
		cairo_pattern_t* pat;
		target = cairo_pdf_surface_create_for_stream(cairoutils_file_write_func, stdout, W, H);
		// render the background image.
		thissurf = cairo_image_surface_create_for_data(img, CAIRO_FORMAT_ARGB32, W, H, W*4);
		pat = cairo_pattern_create_for_surface(thissurf);
		cairo = cairo_create(target);
		cairo_set_source(cairo, pat);
		cairo_paint(cairo);
		cairo_pattern_destroy(pat);
		cairo_surface_destroy(thissurf);
		free(img);
		img = NULL;
	} else {
		if (!img)
			// Allocate a black image.
			img = calloc(4 * W * H, 1);
		target = cairo_image_surface_create_for_data(img, CAIRO_FORMAT_ARGB32, W, H, W*4);
		cairo = cairo_create(target);
	}

	cairo_set_line_width(cairo, lw);
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_GRAY);
	cairo_set_source_rgb(cairo, r, g, b);
    
    marker = cairoutils_parse_marker(shape);
    if (marker == -1) {
        fprintf(stderr, "No such marker: %s\n", shape);
        marker = 0;
    }

    //
    if (background) {
        cairo_save(cairo);
        cairo_set_line_width(cairo, lw+2.0);
        cairo_set_source_rgba(cairo, br, bg, bb, 0.75);
        for (i=n; i<Nxy; i++) {
            double x = starxy_getx(xy, i) + 0.5 - xoff;
            double y = starxy_gety(xy, i) + 0.5 - yoff;
            cairoutils_draw_marker(cairo, marker, x, y, rad);
            cairo_stroke(cairo);
        }
        cairo_restore(cairo);
    }

	// Draw markers.
	for (i=n; i<Nxy; i++) {
		double x = starxy_getx(xy, i) + 0.5 - xoff;
		double y = starxy_gety(xy, i) + 0.5 - yoff;
        cairoutils_draw_marker(cairo, marker, x, y, rad);
		cairo_stroke(cairo);
	}

    // Convert image for output...
	if (img)
		cairoutils_argb32_to_rgba(img, W, H);

	if (ppmoutput) {
        if (cairoutils_write_ppm(outfn, img, W, H)) {
            fprintf(stderr, "Failed to write PPM.\n");
            exit(-1);
        }
	} else if (pdfoutput) {
		cairo_surface_flush(target);
		cairo_surface_finish(target);
		cairoutils_surface_status_errors(target);
		cairoutils_cairo_status_errors(cairo);
	} else {
		// PNG
        if (cairoutils_write_png(outfn, img, W, H)) {
            fprintf(stderr, "Failed to write PNG.\n");
            exit(-1);
        }
    }

    starxy_free(xy);
	cairo_surface_destroy(target);
	cairo_destroy(cairo);
    free(img);

	return 0;
}