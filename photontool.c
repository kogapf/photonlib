#include<stdint.h>
#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
#include<inttypes.h>
#include<string.h>
#include<errno.h>

#include<sys/sysinfo.h>
#include<sys/syscall.h>
#include<sys/stat.h>

#include<png.h>

#include<pthread.h>



// all values in multiples of bytes
#define PHOTON_HEADER_LENGTH 112
#define PHOTON_PREVIEW_IMAGE_HEADER_LENGTH 32
#define STRING_LEN 1000

/* 
 * The photon file format seems to be identical with the cbddlp file format.
 */

	struct pt_args {
		char * ifile;
		struct photon_layer_header * meta;
		int start;
		int end;
		int aa;
		char * filename_base;
		int thread_number;
	} args[4];

// this is only for version  2
struct photon_header {
	uint32_t magic;
	uint32_t version;
	float bed_x;
	float bed_y;

	float bed_z;
	uint32_t _unnamed5;
	uint32_t _unnamed6;
	float cost;
	
	float layer_height;
	float exposure_time;
	float exposure_time_bottom;
	float off_time;

	uint32_t bottom_layers;
	uint32_t resolution_x;
	uint32_t resolution_y;
	uint32_t preview_highres_addr;

	uint32_t layers_addr;
	uint32_t num_layers;
	uint32_t preview_lowres_addr;
	uint32_t print_time;

	uint32_t projection_type;
	uint32_t light_pwm;
	uint32_t light_pwm_bottom;
	uint32_t anti_aliasing;

	uint32_t _unnamed25;
	uint32_t _unnamed26;
};

struct photon_preview_header {
	uint32_t resolution_x;
	uint32_t resolution_y;
	uint32_t image_offset;
	uint32_t image_length;
	uint32_t _unused5;
	uint32_t _unused6;
	uint32_t _unused7;
	uint32_t _unused8;
};

struct photon_layer_header {
	float layer_height;
	float exposure_time;
	float off_time;
	uint32_t addr;
	uint32_t length;
	uint32_t padding[4];
};

int print_photon_header(FILE * stream, struct photon_header *h)
{
	fprintf(stream,"=== Photon Header ===\n");
	fprintf(stream,"Magic number: %08x\n", h->magic);
	fprintf(stream,"Version: %i\n", h->version);
	fprintf(stream,"Bed X: %f\n", h->bed_x);
	fprintf(stream,"Bed Y: %f\n", h->bed_y);
	fprintf(stream,"Bed Z: %f\n", h->bed_z);
	fprintf(stream,"unnamed5[0]: %08x\n", h->_unnamed5);
	fprintf(stream,"unnamed5[1]: %08x\n", h->_unnamed6);
	fprintf(stream,"Cost per liter resin: %f\n", h->cost);
	fprintf(stream,"Layer Height: %f\n", h->layer_height);
	fprintf(stream,"Exposure Time: %f\n", h->exposure_time);
	fprintf(stream,"Exposure Time Bottom: %f\n", h->exposure_time_bottom);
	fprintf(stream,"Off time: %f\n", h->off_time);
	fprintf(stream,"Bottom layers: %i\n", h->bottom_layers);
	fprintf(stream,"Resolution X: %i\n", h->resolution_x);
	fprintf(stream,"Resolution Y: %i\n", h->resolution_y);
	fprintf(stream,"Preview highres address: %08x\n", h->preview_highres_addr);
	//fprintf(stream,"Preview highres offset: %08x\n", h->preview_highres_offset);
	fprintf(stream,"Offset layers: %i\n", h->layers_addr);
	fprintf(stream,"Number layers: %i\n", h->num_layers);
	fprintf(stream,"Preview lowres address: %08x\n", h->preview_lowres_addr);
	//fprintf(stream,"Preview lowres offset: %08x\n", h->preview_lowres_offset);
	fprintf(stream,"Print time: %i [seconds]\n", h->print_time);
	fprintf(stream,"Projection type: %i\n", h->projection_type);
	fprintf(stream,"Anti aliasing: %i\n", h->anti_aliasing);
	fprintf(stream,"Light PWM: %i\n", h->light_pwm);
	fprintf(stream,"Light PWM bottom: %i\n", h->light_pwm_bottom);
	fprintf(stream,"unnamed25: %08x\n", h->_unnamed25);
	fprintf(stream,"unnamed26: %08x\n", h->_unnamed26);
	return 1;
}

int print_photon_preview_header(FILE * stream, struct photon_preview_header *p)
{
	fprintf(stream,"=== Photon Preview Image Header ===\n");
	fprintf(stream,"Resolution X: %i\n", p->resolution_x);
	fprintf(stream,"Resolution Y: %i\n", p->resolution_y);
	fprintf(stream,"Image offset: %08x\n", p->image_offset);
	fprintf(stream,"Image length: %08x\n", p->image_length);
	return 1;
}

int print_photon_layer_header(FILE * stream, struct photon_layer_header * l)
{
	fprintf(stream,"layer_height=%f\n",l->layer_height);
	fprintf(stream,"exposure_time=%f\n",l->exposure_time);
	fprintf(stream,"off_time=%f\n",l->off_time);
	fprintf(stream,"addr=%i\n",l->addr);
	fprintf(stream,"length=%i\n",l->length);
	fprintf(stream,"padding[0]=%i\n",l->padding[0]);
	fprintf(stream,"padding[1]=%i\n",l->padding[1]);
	fprintf(stream,"padding[2]=%i\n",l->padding[2]);
	fprintf(stream,"padding[3]=%i\n",l->padding[3]);
	return 1;
}

int read_photon_preview_image(FILE * file, struct photon_preview_header * p, char * data)
{
	//char * data = malloc(sizeof(char) * p->resolution_x * p->resolution_y * 3);
	fseek(file, p->image_offset, SEEK_SET);
	char * rledata = malloc(sizeof(char) * p->image_length);
	fread(rledata, 1, p->image_length, file);
	int i, j, color;
	uint32_t length = 1;
	int pixel = 0;
	//FILE * fp = fopen("rawpreviewfile", "w+");
	//fwrite(rledata, sizeof(char), p->image_length, fp);
	//fclose(fp);

	uint32_t byte;
	uint8_t r, g, b, a;
	a = 255;
	i = 0;
	while( i < p->image_length ) {
		byte = (rledata[i] & 0xff) | ((rledata[i + 1] << 8) & 0xff00);
		i += 2;
		/*
		r = (uint8_t) ((float) ((byte >> 11) & 0x1f)) / 31 * 255;
		g = (uint8_t) ((float) ((byte >> 6) & 0x1f)) / 31 * 255;
		b = (uint8_t) ((float) ((byte) & 0x1f)) / 31 * 255;
		*/
		r = ((byte >> 11) & 0x1f) / 31.0 * 255;
		g = ((byte >> 6) & 0x1f) / 31.0 * 255;
		b = ((byte) & 0x1f) / 31.0 * 255;

		//printf("Raw color data: %i, %i, %i\n", (byte >> 11) & 0x1f, (byte >> 6) & 0x1f, (byte & 0x1f));

		// X bit
		if (byte & 0x0020) {
			length = 1 + (((rledata[i] & 0xff) | ((rledata[i + 1] << 8) & 0xff00)) & 0x0fff);
			i += 2;
		} else
			length = 1;

		//printf("Abs pos: %i; Read color (%i, %i, %i, %i) with length %i\n", pixel, r, g, b, a, length);

		/*
		if (dataiterator > 300*400*4 - 5) {
			printf("Max imagesize reached.");
			break;
		}
		*/

		for (j = 0; j < length; j++) {
			data[pixel++] = r;
			data[pixel++] = g;
			data[pixel++] = b;
			data[pixel++] = a;
		}
	}
	printf("Wrote %i bytes.\n", pixel);
	free(rledata);
	//free(data);
	return 1;
}

int write_image(char * filename, int width, int height, char * data) 
{
  int y;

  FILE *fp = fopen(filename, "wb");
  if(!fp) abort();
  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) abort();
  png_infop info = png_create_info_struct(png);
  if (!info) abort();
  if (setjmp(png_jmpbuf(png))) abort();
  png_init_io(png, fp);

  // Output is 8bit depth, RGB format.
  png_set_IHDR(
    png,
    info,
    width, height,
    8,
    PNG_COLOR_TYPE_RGBA,
    PNG_INTERLACE_NONE,
    PNG_COMPRESSION_TYPE_DEFAULT,
    PNG_FILTER_TYPE_DEFAULT //TODO: could probably be optimized, but I don't know how
  );
  png_write_info(png, info);

  // To remove the alpha channel for PNG_COLOR_TYPE_RGB format,
  // Use png_set_filler().
  //png_set_filler(png, 0, PNG_FILLER_AFTER);
  int i;
  unsigned char ** row_pointers = malloc(sizeof(char *) * height);
  for (i = 0; i < height; i++){
	  row_pointers[i] = &data[i*width*4];
  }

  if (!row_pointers) abort();

  png_write_image(png, (png_bytepp) row_pointers);
  png_write_end(png, NULL);

 /* 
  for(int y = 0; y < height; y++) {
    free(row_pointers[y]);
  }
  */
  free(row_pointers);

  fclose(fp);

  png_destroy_write_struct(&png, &info);
}

int write_image_grayscale(char * filename, int width, int height, char * data) 
{
  int y;

  FILE *fp = fopen(filename, "wb");
  if(!fp) abort();
  png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png) abort();
  png_infop info = png_create_info_struct(png);
  if (!info) abort();
  if (setjmp(png_jmpbuf(png))) abort();
  png_init_io(png, fp);

  // Output is 8bit depth, RGBA format.
  png_set_IHDR(
    png,
    info,
    width, height,
    8,
    PNG_COLOR_TYPE_GRAY,
    PNG_INTERLACE_NONE,
    PNG_COMPRESSION_TYPE_DEFAULT,
    PNG_FILTER_TYPE_DEFAULT
  );
  png_write_info(png, info);

  // To remove the alpha channel for PNG_COLOR_TYPE_RGB format,
  // Use png_set_filler().
  // png_set_filler(png, 0, PNG_FILLER_AFTER);
  int i;
  unsigned char ** row_pointers = malloc(sizeof(char *) * height);
  for (i = 0; i < height; i++){
	  row_pointers[i] = &data[i*width];
  }

  if (!row_pointers) abort();

  png_write_image(png, (png_bytepp) row_pointers);
  png_write_end(png, NULL);

 /* 
  for(int y = 0; y < height; y++) {
    free(row_pointers[y]);
  }
  free(row_pointers);
  */

  fclose(fp);

  png_destroy_write_struct(&png, &info);
}

int read_photon_preview_header(FILE * file, long int addr, struct photon_preview_header * p)
{
	fseek(file, addr, SEEK_SET);
	fread(p, sizeof(uint8_t), sizeof(struct photon_preview_header), file);
	return 1;
}

int read_photon_layer_header(FILE * file, long int addr, struct photon_layer_header * p)
{
	fseek(file, addr, SEEK_SET);
	fread(p, sizeof(uint8_t), sizeof(struct photon_layer_header), file);
	return 1;
}

int read_photon_layer(FILE * file, struct photon_layer_header * h, char * destination)
{
	//printf("Reading layer at %0x of length %i\n", h->addr, h->length);
	fseek(file, h->addr, SEEK_SET);
	char * buffer = malloc(h->length);
	fread(buffer, 1, h->length, file);

	int j, i, color, length;
	int pixel = 0;
	for (i = 0; i < h->length; i++) {
		color = (buffer[i] >> 7) & 0x01;
		length = (buffer[i] & 0x7f); 

		for (j = 0; j < length; j++) {
			destination[pixel++] = 255*color;
			destination[pixel++] = 255*color;
			destination[pixel++] = 255*color;
			destination[pixel++] = 255;
		}

	}
	//printf("Read image of length %i\n", pixel);
	free(buffer);
	return 0;
}

// TODO: anti aliasing support
int extract_photon_layers_noaa(FILE * file, struct photon_layer_header * h, uint32_t nlayers, char * filename_base)
{
	char * buffer = malloc(10000000); // TODO: this is just a good guess
	char * dest = malloc(2560*1440*4); // TODO: make this not static
	char * filename = malloc(1000); // TODO: preprocessor
	long buflen = 0;

	int n;
	for (n = 0; n < nlayers; n++) {
		if (h->length > buflen) {
			if (realloc(buffer, h->length) == NULL) {
				printf("Couldn't allocate buffer.\n");
				return -1;
			}
			buflen = h->length;
			printf("Reallocted for len %li\n", buflen);
		}
		//printf("Reading layer at %0x of length %i\n", h->addr, h->length);
		fseek(file, h->addr, SEEK_SET);

		fread(buffer, 1, h->length, file);

		int j, i, color, length;
		int pixel = 0;
		for (i = 0; i < h->length; i++) {
			color = (buffer[i] >> 7) & 0x01;
			length = (buffer[i] & 0x7f); 

			for (j = 0; j < length; j++) {
				dest[pixel++] = 255*color;
				dest[pixel++] = 255*color;
				dest[pixel++] = 255*color;
				dest[pixel++] = 255;
			}

		}
		sprintf(filename, "%s_%04i.png", filename_base, n);
		write_image(filename, 1440, 2560, dest);
		//printf("Read image of length %i to file %s\n", pixel, filename);
		read_photon_layer_header(file, h->addr + h->length, h);
		//int read_photon_layer_header(FILE * file, long int addr, struct photon_layer_header * p)
	}
	printf("Max buflen used: %li", buflen);
	free(buffer);
	free(dest);
	free(filename);
	return 0;
}

void * extract_photon_layer(void * args)
{
	struct pt_args * test = (struct pt_args * ) args;
	char * ifile = test->ifile;
	struct photon_layer_header * meta = test->meta;
	int start = test->start;
	int end = test->end;
	int anti_aliasing = test->aa;
	char * filename_base = test->filename_base;
	FILE * file = fopen(ifile, "r");
	long buflen = 250000;
	char * buffer = malloc(buflen); // TODO: this is just a good guess
	char * dest = malloc(2560*1440*4); // TODO: preproc
	int level;
	char * filename = malloc(1000); // TODO: preprocessor
	int pixel;
	int j, i, color, length;
	struct photon_layer_header * h = &meta[start];

	int n;
	for (n = start; n < end; n++) {

		for (level = 0; level < anti_aliasing; level++) {
			if (h->length > buflen) {
				if (realloc(buffer, h->length) == NULL) {
					printf("Couldn't allocate buffer.\n");
					exit(-1);
				}
				buflen = h->length;
				//printf("Reallocted for len %li\n", buflen);
			}
			//printf("Reading layer at %0x of length %i\n", h->addr, h->length);
			fseek(file, h->addr, SEEK_SET);
 
			fread(buffer, 1, h->length, file);
			pixel = 0;
			for (i = 0; i < h->length; i++) {
				color = (buffer[i] >> 7) & 0x01;
				length = (buffer[i] & 0x7f); 

				for (j = 0; j < length; j++) {
					// on the first run clean the image of previous info
					if (level == 0) 
						dest[pixel] = 0;
					dest[pixel++] |= (color << (7 - level));
					/*
					if (level == 7 ) {
						printf("Wrote color value %ui\n", dest[pixel - 2]);
					}
					*/
				}

			}
			read_photon_layer_header(file, h->addr + h->length, h);
		}
		sprintf(filename, "%s_%04i.png", filename_base, n);
		write_image_grayscale(filename, 1440, 2560, dest);
		//printf("Read image of length %i to file %s\n", pixel, filename);
		//read_photon_layer_header(file, h->addr + h->length, h);
		//int read_photon_layer_header(FILE * file, long int addr, struct photon_layer_header * p)

		// this prints the progress
		// it should be moved to the last thread, due to the partitioning scheme
		// used. the last partition is always the largest, as we do divide the
		// whole file by the number of threads, wherein to the last segment the
		// remainder is added. its max size may be 2*segment - 1 layers
		if (test->thread_number==0) {
			printf("\rProgress: %03.2f%%", (float) 100 * (n - start) / (float) (end - start));
			fflush(stdout);
		}
	}
	printf("Max buflen used: %li\n", buflen);
	free(buffer);
	free(dest);
	free(filename);
	fclose(file);
	return 0;
}

// encodes to rle and saves in rledata[aa_level]
// imagedata must be a raw 8-bit grayscale image array
// return 0 on success
// NOTE: might just pass the width*height as one parameter ?
// TODO: if we exceed buflen we have a BIG PROBLEM -> allocate separate buffer
// for each sublayer
#define BUFLEN 250000
int write_photon_layer(char * imagedata, int width, int height, char * rledata, int * lengths, int aa_level)
{
	//long buflen = 250000; // TODO: move to precompiler
	// we can't cast into array char[buflen][] type, so we have to be careful
	rledata = (char *) malloc(BUFLEN * aa_level);
	lengths = malloc(sizeof(int) * aa_level);
	int i;
	int length, color; // isn't really a color, but sounds better
	int startcolor;
	length = 1;
	startcolor = imagedata[i] & (0x01 << 7); // most significant bit of color
	int aa;
	for (aa = 0; aa < aa_level; aa++) { 
		int rlecounter = 0;
		for (i = 1; i < width * height; i++) {
			color = imagedata[i] & (0x01 << (7 - aa));
			if (startcolor == color || length < 0x7d) { // if the color stays the same, we go to the next color, or the maximum length is not yet reached
				length++;
			} else { // we set the startcolor to the new color and write the result
				startcolor = color;
				int j;
				for (j = 0; j < length; j++) {
					rledata[aa*BUFLEN + rlecounter++] = ((color << 7) & 0xf) & (length & 0x7f);
				}
			}
		}
		lengths[aa] = rlecounter;
	}
	return 0;
}

int extract_photon_layers(FILE * file, struct photon_layer_header * h, uint32_t nlayers, int anti_aliasing, char * filename_base)
{
	long buflen = 250000;
	char * buffer = malloc(buflen); // TODO: this is just a good guess
	char * dest = malloc(2560*1440*4); // TODO: preproc
	int level;
	char * filename = malloc(1000); // TODO: preprocessor
	int pixel;
	int j, i, color, length;

	int n;
	for (n = 0; n < nlayers; n++) {

		for (level = 0; level < anti_aliasing; level++) {
			if (h->length > buflen) {
				if (realloc(buffer, h->length) == NULL) {
					printf("Couldn't allocate buffer.\n");
					return -1;
				}
				buflen = h->length;
				//printf("Reallocted for len %li\n", buflen);
			}
			//printf("Reading layer at %0x of length %i\n", h->addr, h->length);
			fseek(file, h->addr, SEEK_SET);
 
			fread(buffer, 1, h->length, file);
			pixel = 0;
			for (i = 0; i < h->length; i++) {
				color = (buffer[i] >> 7) & 0x01;
				length = (buffer[i] & 0x7f); 

				for (j = 0; j < length; j++) {
					// on the first run clean the image of previous info
					if (level == 0) 
						dest[pixel] = 0;
					dest[pixel++] |= (color << (7 - level));
					/*
					if (level == 7 ) {
						printf("Wrote color value %ui\n", dest[pixel - 2]);
					}
					*/
				}

			}
			read_photon_layer_header(file, h->addr + h->length, h);
		}
		sprintf(filename, "%s_%04i.png", filename_base, n);
		write_image_grayscale(filename, 1440, 2560, dest);
		//printf("Read image of length %i to file %s\n", pixel, filename);
		//read_photon_layer_header(file, h->addr + h->length, h);
		//int read_photon_layer_header(FILE * file, long int addr, struct photon_layer_header * p)
	}
	printf("Max buflen used: %li\n", buflen);
	free(buffer);
	free(dest);
	free(filename);
	return 0;
}

int read_photon_header(FILE * file, struct photon_header * h)
{
	fseek(file, 0, SEEK_SET);
	fread(h, sizeof(struct photon_header), 1, file);
	return 1;
}

/* reads all metadata of every layer into a table */
int read_photon_layer_metadata(FILE * f, struct photon_header * h, struct photon_layer_header * dest)
{
	int addr = h->layers_addr;
	int i;
	for (i = 0; i < h->num_layers*h->anti_aliasing; i++) {
		//printf("Reading layer %i\n", i);
		//fseek(f, addr, SEEK_SET);
			//read_photon_layer_header(file, h->addr + h->length, h);
		read_photon_layer_header(f, addr, &dest[i]);
		//fread(&(*dest[i]), sizeof(uint8_t), sizeof(struct photon_layer_header), f);
		//print_photon_layer_header(dest[i]);
		addr = dest[i].addr + dest[i].length;
	}
}


int photon_header_to_text(struct photon_header h, char * filename)
{

}

int check_sanity_metadata(struct photon_header * h, struct photon_preview_header * p1, struct photon_preview_header * p2, struct photon_layer_header ** l)
{
}


int main(int argc, char *argv[])
{
	int opt;
	int analyze_flag = 0;
	int extract_flag = 0;
	int test_flag = 0;
	int read_length = 0;
	int start;
	char * input_file = NULL;
	char * output_file;
	char * data;
	FILE * file;
	int workers = 0;
	while ((opt = getopt(argc, argv, "eo:j:i:al:s:t:x")) != -1 ) {
		switch (opt) {
			case 'o':
				output_file = optarg;
			case 'a':
				analyze_flag = 1;
				break;
			case 'i':
				input_file = optarg;
				//input_file = (char *) malloc(strlen(optarg));
				//strcpy(input_file, optarg);
				break;
			case 'l':
				read_length = atoi(optarg);
				break;
			case 's': // start
				start = atoi(optarg);
				break;
			case 'e': // extract
				extract_flag = 1;
				break;
			case 'x': // test, multi thread
				test_flag = 1;
				break;
			case 'j': // numer of threads
				workers = atoi(optarg);
				break;
		}
	}

	if (extract_flag) { // -e
		if (input_file == NULL) {
			printf("No input file specified. Aborting.\n");
			return -1;
		}

		if( !access(output_file, F_OK )) {
			printf("Output directory already exists. Aborting.\n");
			exit(-1);
		} else {
			mkdir(output_file, 0700);
		}


		FILE * f = fopen(input_file, "r");
		struct photon_header * h = malloc(sizeof(struct photon_header));
		struct photon_preview_header * p1 = malloc(sizeof(struct photon_preview_header));
		struct photon_preview_header * p2 = malloc(sizeof(struct photon_preview_header));
		struct photon_layer_header * l = NULL; //TODO: define maxlayers preproc

		read_photon_header(f, h);
		read_photon_preview_header(f, h->preview_lowres_addr, p1);
		read_photon_preview_header(f, h->preview_highres_addr, p2);

		/* Adjustment for version 1 photon file format.
		 * set anti-aliasing from 0 -> 1
		 */
		if (h->anti_aliasing == 0) {
			h->anti_aliasing == 1; 
		}

		l = (struct photon_layer_header *) malloc(sizeof(struct photon_layer_header) * h->anti_aliasing * h->num_layers);
		/*
		l = malloc(sizeof(char*) * h->anti_aliasing * h->num_layers);
		int k;
		for (k = 0; k < h->anti_aliasing * h->num_layers; k++) {
			l[k] = malloc(sizeof(struct photon_layer_header));
		}
		*/
		read_photon_layer_metadata(f, h, l);

		FILE * o = NULL;

		char * header_file_name = malloc(STRING_LEN);
		sprintf(header_file_name, "%s/%s_photon_header.txt",output_file, output_file);
		char * preview_header_1_file_name = malloc(STRING_LEN);
		sprintf(preview_header_1_file_name, "%s/%s_photon_header_1.txt",output_file, output_file);
		char * preview_header_2_file_name = malloc(STRING_LEN);
		sprintf(preview_header_2_file_name, "%s/%s_photon_header_2.txt",output_file, output_file);
		char * preview_image_1_file_name = malloc(STRING_LEN);
		sprintf(preview_image_1_file_name, "%s/%s_photon_preview_1",output_file, output_file);
		char * preview_image_2_file_name = malloc(STRING_LEN);
		sprintf(preview_image_2_file_name, "%s/%s_photon_preview_2",output_file, output_file);
		char * layer_image_base = malloc(STRING_LEN);
		sprintf(layer_image_base, "%s/%s_layer",output_file, output_file);
		char * layer_header_file_name = malloc(STRING_LEN);
		sprintf(layer_header_file_name, "%s/%s_layers.txt",output_file, output_file);

		o = fopen(header_file_name, "a");
		if (!o) {
			printf("Failed to open file %s. Errno: %i\n", header_file_name, errno);
			exit(-1);
		}
		print_photon_header(o, h);
		fclose(o);

		o = fopen(preview_header_1_file_name, "a");
		print_photon_preview_header(o, p1);
		fclose(o);

		o = fopen(preview_header_2_file_name, "a");
		print_photon_preview_header(o, p2);
		fclose(o);

		o = fopen(layer_header_file_name, "a");
		int layer = 0;
		for (layer = 0; layer < h->num_layers * h->anti_aliasing; layer++) {
			fprintf(o, "META INFORMATION FOR LAYER %i [%i]\n", layer/h->anti_aliasing, layer % h->anti_aliasing);
			print_photon_layer_header(o, &l[layer]);
		}
		fclose(o);

		fclose(f);

		free(header_file_name);
		free(preview_header_1_file_name);
		free(preview_header_2_file_name);
		free(preview_image_1_file_name);
		free(preview_image_2_file_name);
		free(layer_image_base);
		free(layer_header_file_name);

		free(l);
		free(p1);
		free(p2);

		return 0;

	}


	if (test_flag) { // -x

	struct photon_header * h;
	file = fopen(input_file, "r");
	if ( file == NULL ) {
		fprintf(stderr, "Error opening file.\n");
		return 1;
	}
	h = malloc(sizeof(struct photon_header));
	if (read_photon_header(file, h) != 1) {
		fprintf(stderr, "Couldn't read photon header.\n");
		return -1;
	}

	/* Adjustment for version 1 photon file format.
	 * set anti-aliasing from 0 -> 1
	 */
	if (h->anti_aliasing == 0) {
		h->anti_aliasing == 1; 
	}

	struct photon_preview_header *p, *p2;
	p = malloc(sizeof(struct photon_preview_header));
	if (read_photon_preview_header(file, h->preview_lowres_addr, p) != 1) {
		fprintf(stderr, "Couldn't read photon preview image header.\n");
		return -1;
	}

	p2 = malloc(sizeof(struct photon_preview_header));
	if (read_photon_preview_header(file, h->preview_highres_addr, p2) != 1) {
		fprintf(stderr, "Couldn't read photon preview image header.\n");
		return -1;
	}

	FILE * photon_header_dest = fopen("photon_header_test", "w");
	print_photon_header(photon_header_dest, h);	
	fclose(photon_header_dest);
	//file = fopen(input_file, "r");
	// preview image 1

	FILE * photon_preview_header_dest = fopen("photon_preview1_test", "w");
	print_photon_preview_header(photon_preview_header_dest, p);
	fclose(photon_preview_header_dest);

	printf("About to read image.\n");
	data = malloc(p->resolution_x * p->resolution_y * 4);
	read_photon_preview_image(file, p, data);
	printf("Image successfully read.\n");
	write_image("testim.png", p->resolution_x, p->resolution_y, data);
	printf("Image successfully written.\n");

	/* preview image 2 */
	free(data);
	data = malloc(p2->resolution_x * p2->resolution_y * 4);
	FILE * photon_preview_header_dest2 = fopen("photon_preview2_test", "w");
	print_photon_preview_header(photon_preview_header_dest2, p2);
	fclose(photon_preview_header_dest2);
	printf("About to read image.\n");
	read_photon_preview_image(file, p2, data);
	printf("Image successfully read.\n");
	write_image("testim2.png", p2->resolution_x, p2->resolution_y, data);
	printf("Image successfully written.\n");
	free(data);

	/* layer */
	struct photon_layer_header * l = malloc(sizeof(struct photon_layer_header));
	read_photon_layer_header(file, h->layers_addr, l);

//	char * destination = malloc(1440*2560*4);
//	read_photon_layer(file, l, destination);
	//extract_photon_layers_noaa(file, l, h->num_layers*8, "test");
//int extract_photon_layers(FILE * file, struct photon_layer_header * h, uint32_t nlayers, int anti_aliasing, char * filename_base)
	//extract_photon_layers(file, l, h->num_layers, 8, "test");
	// write_image("testlayer.png", h->resolution_x, h->resolution_y, destination);


	/*
	 * I DON'T UNDERSTAND WHY THE FUCK I CAN'T JUST ALLOCATE ONE LARGE
	 * CONTINUOUS CHUNK OF MEMORY TO STORE ALL THE LAYER HEADERS IN...
	 */
	struct photon_layer_header * layers = NULL; //TODO: define maxlayers preproc
	//layers = (struct photon_layer_header) malloc(sizeof(photon_layer_header) * h->anti_aliasing * h->num_layers);
	layers = (struct photon_layer_header *) malloc(sizeof(struct photon_layer_header) * h->anti_aliasing * h->num_layers);
	/*
	layers = malloc(sizeof(char*) * h->anti_aliasing * h->num_layers);
	int k;
	for (k = 0; k < h->anti_aliasing * h->num_layers; k++) {
		layers[k] = malloc(sizeof(struct photon_layer_header));
	}
	*/

	//layers = (struct photon_layer_header **) malloc(sizeof(struct photon_layer_header) * h->anti_aliasing * h->num_layers);
	read_photon_layer_metadata(file, h, layers);

	/*
	int i;
	for (i = 0; i < h->anti_aliasing * h->num_layers; i++) {
		print_photon_layer_header(layers[i]);
	}
	*/

	if (!workers) 
		workers = get_nprocs();
	printf("Using %d threads\n", workers);
	int segment = (h->num_layers - 1) / 4; // TODO: determine number of workers automatically
	// -1, to prevent start == end for last worker
	int s = 0;

	
	pthread_t * threads = malloc(workers*sizeof(pthread_t));
	int w;
	int start, end;
	for (w = 0; w < workers; w++) {
		if (w == workers - 1)
			end = h->num_layers;
		else
			end = segment*(w + 1);
		start = segment*w;
		args[w].ifile = input_file;
		args[w].meta = layers;
		args[w].start = start;
		args[w].end = end;
		args[w].aa = h->anti_aliasing;
		if (output_file != NULL) {
			args[w].filename_base = output_file;
		} else {
			args[w].filename_base = "DEFAULT";
		}
		args[w].thread_number = w;
		pthread_create(&threads[w], NULL, extract_photon_layer, &args[w]);
		printf("Started thread nr. %i. Works on layers %i to %i.\n", w, start, end);
	}
	
	for (w = 0; w < workers; w++) {
		pthread_join(threads[w], NULL);
	}
	free(l);

	// int extract_photon_layer(char * ifile, struct photon_layer_header ** meta, int start, int end, int anti_aliasing, char * filename_base)




	return 0;
	}



	if (analyze_flag) {
		uint32_t target; 
		uint32_t target_le; 
		printf("Will perform basic analysis on file %s.\n", input_file);
		printf("Assuming photon version 3 file.");
		file = fopen(input_file, "r");
		if ( file == NULL ) {
			fprintf(stderr, "Error opening file.\n");
			return 1;
		}

		int i;
		if ( read_length == 0 ) {
			read_length = 360;
			printf("No length specified. Reading a default of %i quad words (64 bits).\n", read_length);
		}
		for ( i = 0; 1 ; i++ ) {
			fread(&target, sizeof(uint32_t), 1, file);
			target_le = __builtin_bswap32(target);
			if (i < start ) 
				continue;
			else if ( i > start + read_length )
				break;

			if (i % 4 == 0)  {
				printf("\n");
			}

			printf("=== Reading quad word no. %3i (pos %3i) ===\t", i, i*4);
			printf("be int: %08x (%10i)\t", target, target);
			printf("le int: %08x\t", target_le);
			printf("be float: %017.5f\t", *((float*)&target));
			printf("le float: %017.5f\t", *((float*)&target_le));

			switch (i) {
				case 0:
					printf("\tMAGIC NUMBER");
					break;
				case 1:
					printf("\tVERSION");
					break;
				case 2:
					printf("\tBED_X");
					break;
				case 3:
					printf("\tBED_Y");
					break;
				case 4:
					printf("\tBED_Z");
					break;
				case 5:
					printf("\tUNUSED");
					break;
				case 6:
					printf("\tUNUSED");
					break;
				case 7:
					printf("\tLAYER COST");
					break;
				case 8:
					printf("\tLAYER HEIGHT");
					break;
				case 9:
					printf("\tEXPOSURE TIME");
					break;
				case 10:
					printf("\tBOTTOM EXPOSURE TIME");
					break;
				case 11:
					printf("\tOFF TIME");
					break;
				case 12:
					printf("\tBOTTOM LAYER COUNT");
					break;
				case 13:
					printf("\tRESOLUTION X");
					break;
				case 14:
					printf("\tRESOLUTION Y");
					break;
				case 15:
					printf("\tPREVIEW HIGHRES (pointer)");
					break;
				case 16:
					printf("\tOFFSET LAYERS (pointer)");
					break;
				case 17:
					printf("\tNUMBER LAYERS");
					break;
				case 18:
					printf("\tPREVIEW LOWRES (pointer)");
					break;
			}
			printf("\n");
		}

	}

	fclose(file);

	printf("DONE");
	return 0;
}

