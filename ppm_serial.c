/* PPM Interface modified from: http://stackoverflow.com/questions/2693631/read-ppm-file-and-store-it-in-an-array-coded-with-c */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <dirent.h>

typedef struct {
     unsigned char red,green,blue;
} PPMPixel;

typedef struct {
     int x, y;
     PPMPixel *data;
} PPMImage;

// Filter struct. See http://lodev.org/cgtutor/filtering.html for more info
typedef struct {
     int x, y;
     double data[25];
     double factor;
     double bias;
} Filter;

typedef struct {
     int x, y, z;
     double data[125];
     double factor;
     double bias;
} Filter_3D;

#define CREATOR "RVCE"
#define RGB_COMPONENT_COLOR 255

int get_num_frames(){
     int file_count = 0;
     DIR * dirp;
     struct dirent * entry;

     dirp = opendir("infiles"); /* There should be error handling after this */
     if (dirp == NULL)
          exit(EXIT_FAILURE);
     while ((entry = readdir(dirp)) != NULL) {
          if (entry->d_type == DT_REG) { /* If the entry is a regular file */
               file_count++;
          }
     }
     closedir(dirp);
     return file_count;
}

void freeImage(PPMImage * image) {
    if(image) {
        if(image->data)
            free(image->data);
        free(image);
    }
}

static PPMImage *readPPM(const char *filename)
{
         char buff[16];
         PPMImage *img;
         FILE *fp;
         int c, rgb_comp_color;
         //open PPM file for reading
         fp = fopen(filename, "rb");
         if (!fp) {
              // fprintf(stderr, "Hey! Unable to open file '%s'\n", filename);
              // exit(1);
            return NULL;
         }

         //read image format
         if (!fgets(buff, sizeof(buff), fp)) {
              perror(filename);
              exit(1);
         }

    //check the image format
    if (buff[0] != 'P' || buff[1] != '6') {
         fprintf(stderr, "Invalid image format (must be 'P6')\n");
         exit(1);
    }

    //alloc memory form image
    img = (PPMImage *)malloc(sizeof(PPMImage));
    if (!img) {
         fprintf(stderr, "Unable to allocate memory\n");
         exit(1);
    }

    //check for comments
    c = getc(fp);
    while (c == '#') {
        while (getc(fp) != '\n')
            ;
        c = getc(fp);
    }

    ungetc(c, fp);
    //read image size information
    if (fscanf(fp, "%d %d", &img->x, &img->y) != 2) {
         fprintf(stderr, "Invalid image size (error loading '%s')\n", filename);
         exit(1);
    }

    //read rgb component
    if (fscanf(fp, "%d", &rgb_comp_color) != 1) {
         fprintf(stderr, "Invalid rgb component (error loading '%s')\n", filename);
         exit(1);
    }

    //check rgb component depth
    if (rgb_comp_color!= RGB_COMPONENT_COLOR) {
         fprintf(stderr, "'%s' does not have 8-bits components\n", filename);
         exit(1);
    }

    while (fgetc(fp) != '\n') ;
    //memory allocation for pixel data
    img->data = (PPMPixel*)malloc(img->x * img->y * sizeof(PPMPixel));

    if (!img) {
         fprintf(stderr, "Unable to allocate memory\n");
         exit(1);
    }

    //read pixel data from file
    if (fread(img->data, 3 * img->x, img->y, fp) != img->y) {
         fprintf(stderr, "Error loading image '%s'\n", filename);
         exit(1);
    }

    fclose(fp);
    return img;
}

void writePPM(const char *filename, PPMImage *img)
{
    FILE *fp;
    //open file for output
    fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Unable to open file '%s'\n", filename);
        exit(1);
    }

    //write the header file
    //image format
    fprintf(fp, "P6\n");

    //comments
    fprintf(fp, "# Created by %s\n",CREATOR);

    //image size
    fprintf(fp, "%d %d\n",img->x,img->y);

    // rgb component depth
    fprintf(fp, "%d\n",RGB_COMPONENT_COLOR);

    // pixel data
    fwrite(img->data, 3 * img->x, img->y, fp);
    fclose(fp);
}

int print = 0;
// filter a single PPM image
void filterPPM(PPMImage *img, Filter f)
{
    if(!img) // return if we don't have a valid image to work on
        return;

    // creating a new output image since we can't "contaminate" the input by overwriting
    // with the output
    PPMPixel * new_data = (PPMPixel*)malloc(img->x * img->y * sizeof(PPMPixel));

    int img_x, img_y; // current pixel coordinates in the input image we are workin gon
    int f_x, f_y; // current coordinates in the filter
    int halo_x, halo_y; // current coordinates of the halo cells

    // loop over all pixels in image
    for(img_x = 0; img_x < img->x; img_x++) {
        for(img_y = 0; img_y < img->y; img_y++) {
            // accumulate red, green, and blue vals for this pixel
            int red = 0, green = 0, blue = 0;
            int img_idx = img_y * img->x + img_x;

            // loop over surrounding pixels
            for(f_x = 0; f_x < f.x; f_x++) {
                for(f_y = 0; f_y < f.y; f_y++) {
                    // get absolute locations of surrounding pixels
                    halo_x = img_x + (f_x - f.x / 2);
                    halo_y = img_y + (f_y - f.y / 2);

                    // check to make sure halo_x and halo_y are valid coordinates
                    if(halo_x >= 0 && halo_x < img->x && halo_y >= 0 && halo_y < img->y) {
                        // get current color channels
                        int curr_red = img->data[halo_y * img->x + halo_x].red;
                        int curr_green = img->data[halo_y * img->x + halo_x].green;
                        int curr_blue = img->data[halo_y * img->x + halo_x].blue;

                        // get current kernel value
                        int fval = f.data[f_x * f.x + f_y];

                        // accumulate filter * halo
                        red += curr_red * fval;
                        green += curr_green * fval;
                        blue += curr_blue * fval;
                    }
                }
            }

            
            // apply filter factor and bias, and write resultant pixel back to img (being cautious of overflow and underflow)
            new_data[img_y * img->x + img_x].red = fmin( fmax( (int)(f.factor * red + f.bias), 0), 255);
            new_data[img_y * img->x + img_x].green = fmin( fmax( (int)(f.factor * green + f.bias), 0), 255);
            new_data[img_y * img->x + img_x].blue = fmin( fmax( (int)(f.factor * blue + f.bias), 0), 255);
        }
    }

    // link img data to new_data
    img->data = new_data;
    
}

void colorPPM(PPMImage *img)
{
    int img_x, img_y;
    for(img_x = 0; img_x < img->x; img_x++) {
        for(img_y = 0; img_y < img->y; img_y++) {
            int avg = (img->data[img_y * img->x + img_x].red + img->data[img_y * img->x + img_x].green + img->data[img_y * img->x + img_x].blue) / 3;
            img->data[img_y * img->x + img_x].red = avg;
            img->data[img_y * img->x + img_x].green = avg;
            img->data[img_y * img->x + img_x].blue = avg;
        }
    }
}
double processImage(PPMImage * images, Filter f, int stride_len) {
    double time_spent = 0.0;
    clock_t begin, end;

    char instr[80];
    char outstr[80];
    int i = 0, j = 0;
    //int numFrames = 301;
    int numFrames = get_num_frames();
    int numChunks = numFrames / stride_len;
    if(numFrames % stride_len) numChunks++;

    for(i = 0; i < numChunks; i ++) {

        // read 1 chunk of stride_len frames into images[]
        for(j = 0; j < stride_len && j + i*stride_len < numFrames; j++) {
            sprintf(instr, "infiles/tmp%03d.ppm", i*stride_len + j + 1);
            PPMImage * img = readPPM(instr);
            if(img == NULL) {
                printf("All files processed\n");
                return time_spent;
            }
            images[j] = *img;
        }
        
        // process chunk of frames in images[]
        for(j = 0; j < stride_len && j + i*stride_len < numFrames; j++) {
        //for(j = 0; j < 1; j++) {

            PPMImage * img = &images[j];
            
            begin = clock();
#ifdef CONV
            filterPPM(img, f);
#endif /*CONV*/

#ifdef BANDW 
            colorPPM(img);
#endif /*BANDW*/


            end = clock();
            time_spent += ((double)(end - begin) / CLOCKS_PER_SEC);
        }
        
        

        // write 1 chunk of stride_len frames from images[]
        for(j = 0; j < stride_len && j + i*stride_len < numFrames; j++) {
            sprintf(outstr, "outfiles/tmp%03d.ppm", i*stride_len + j + 1);
            writePPM(outstr, &images[j]);
        }
    }

    return time_spent;
}

// ppm <stride_len> (<max_frames>)
int main(int argc, char *argv[]){
    int stride_len;
    if ( argc < 2 ) /* argc should be at least 2 for correct execution */
    {
        /* We print argv[0] assuming it is the program name */
        printf("Using default stride length of 20 frames\n");
       stride_len = 20;
    } else {
        stride_len = atoi(argv[1]);
    }

    clock_t begin, end;
    double time_spent;   // time spent for each chunk, plus accumulate total at end

    PPMImage images[stride_len];

    Filter edge = {
        .x = 5,
        .y = 5,
        .data = {
            0, 0, 0, 0, 0,
            0, -1, -1, -1, 0,
            0, -1, 8, -1, 0,
            0, -1, -1, -1, 0,
            0, 0, 0, 0, 0
        },

        .factor = 1.0,
        .bias = 0
    };

    Filter emboss = {
        .x = 5,
        .y = 5,
        .data = {
            0, 0, 0, 0, 0,
            0, -2, -1, 0, 0,
            0, -1, 1, 1, 0,
            0, 0, 1, 2, 0,
            0, 0, 0, 0, 0
        },

        .factor = 1.0,
        .bias = 0
    };

    // loop over all frames, in chunks of stride_len
    begin = clock();

    double calc_time = processImage(images, edge, stride_len);

    end = clock();
    time_spent = (double)(end - begin) / CLOCKS_PER_SEC;

    printf("%f seconds spent\n%f seconds spent processing\n", time_spent, calc_time);

    // Combine frames into a single video with ffmpeg
    if (!system(NULL)) { exit (EXIT_FAILURE);}
    char ffmpegString[200];
    sprintf(ffmpegString, "ffmpeg -framerate 24 -i outfiles/tmp%%03d.ppm -c:v libx264 -r 30 -pix_fmt yuv420p ./outfilter.mp4 -hide_banner -loglevel error");
    system (ffmpegString);


    return 0;
}
