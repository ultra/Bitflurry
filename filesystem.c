#include "filesystem.h"

/* Get size in bytes of the specified file */
long fs_getFileSize(char *filename){
    struct stat statBuffer;

    if (stat(filename, &statBuffer) != 0)
    {
        printf("\n\nERROR: Unable to obtain attributes of file %s.", filename);
        return -1;
    }
    return (long) statBuffer.st_size;
}

// split file into chunk and put them in our storage arrays
void fs_putFile(char *filename) {
	long file_size;           			//total size of the file
	long slice_size = CHUNK_SIZE;    	//size of each slice: 4MB
	long last_slice_size;				//size of the last slice
	long cur_slice_size;				//determine whether the size is 4MB or the remainder (last_slice_size)
	long num_slices;	      			//number of slices
	int slice_index;		  			//used in the loop
	int last_slice_index;

	char *file_out = NULL;
	FILE *fp_in;
	FILE *fp_out;

	char buffer[BUFFER_SIZE];
	int bytes_read;
	int bytes_to_write;
	long bytes_written;

	file_size = fs_getFileSize(filename);						// get a size of the file
	num_slices = file_size/slice_size;							// number of slices
	last_slice_index = num_slices + 1;							// index of the last slice
	last_slice_size = file_size - (num_slices*slice_size);		// size of the last slice

	fp_in = fopen(filename, "rb");								// open original file

	int dir_index = 0; 											// our raid dirs indexer
	
	// Make sure there is no duplicate filenames in the database
	while (db_isFileExist(filename)) {
		char *filename_new = malloc((strlen(filename) + 5) * sizeof(char));
		sprintf(filename_new, "%s.new", filename);
		
		filename = filename_new;
	}
	
	int id = 0;
	id = db_insertFile(filename);
	printf("Putting file %s...\n", filename);
	//printf("Using file_id: %d\n", id);
	
	int lastIndex[2];
	db_getLastIndex(lastIndex);
	//printf("Last - Row: %d, Col: %d\n\n", lastIndex[1], lastIndex[0]);

	printf("Progress: 0...");
	for (slice_index = 0; slice_index <= num_slices; slice_index++) {
		// allocate memory for the name of the output files
		file_out = (char *) realloc(file_out, (strlen(DISK_PATH) + strlen(DISK_ARRAY[dir_index]) + toDigit(lastIndex[1]) + 3) * sizeof(char));    //sizeof(char) = 1 byte

		sprintf (file_out, "%s/%s/%d", DISK_PATH, DISK_ARRAY[dir_index], lastIndex[1]);  //concatenate names for the new output: movie.mp4.1 , movie.mp4.2, ...
		fp_out = fopen(file_out, "wb");						   // create and open a output file
		if (fp_out == NULL) {
			printf("Fatal error: Perhaps the storage doesn't exist or bitflurry has no write permission.");
			break;
		}
		bytes_written = 0;

		// determine whether the size is 4MB or the remainder (last_slice_size)
		if (slice_index == last_slice_index) {
			cur_slice_size = last_slice_size;   // remainder
		}
		else {
			cur_slice_size = slice_size;        // 4MB
		}

		while (bytes_written < cur_slice_size) {
			bytes_to_write = BUFFER_SIZE;
			bytes_read = fread(buffer, 1, bytes_to_write, fp_in);    // read bytes from file to the buffer
			fwrite(buffer, 1, bytes_read, fp_out);					 // write bytes from buffer to the current slice
			bytes_written += bytes_to_write;
		}
		fclose(fp_out);		// close the output file
		
		lastIndex[0]++;
		if (lastIndex[0] >= DISK_TOTAL) { lastIndex[1]++; lastIndex[0] = 0; }
		
		if (db_insertChunk(id, lastIndex[0], lastIndex[1], slice_index) != SQLITE_OK) break;
		//printf("Inserted for order: %d at (%d, %d)\n", slice_index, lastIndex[1], lastIndex[0]);
		printf("%d...", ((slice_index+1)*100)/(num_slices+1));

		dir_index++;
		if (dir_index > DISK_TOTAL-1) dir_index = 0;
	}
	printf("done!\n");
	printf("\nFile transaction completed successfully.\n");
	//printf("\nDB Insert Completed.\n");
	
	fclose(fp_in);			// close the original file	
	free(file_out);			// free the memory
}


void fs_getFile(char *filename, char *outfile) {
	
	char buffer[BUFFER_SIZE];
	int bytes_read;
	long bytes_written;
	
	int to_stdout = 0;

	int i = 0;
	
	bf_file * file = db_getFile(filename);
	
	if (file != NULL) {
		char *chunkfile = NULL;
		
		printf("Getting %s...\n", file->filename);
		//printf("file_id: %d\n", file->_id);
		//printf("filename: %s\n", file->filename);
		//printf("total chunks: %d\n", file->total_chunks);
		//printf("chunks:\n");
		
		FILE *file_out;
		if (strcmp(outfile, "-") == 0) {
			printf("Writing to /dev/stdout\n");
			to_stdout = 1;
			file_out = stdout;
			//file_out = freopen("/dev/null", "wb", stdout);
		} else {
			printf("Writing to %s\n", outfile);
			file_out = fopen(outfile, "wb");
		}
		
		if (!to_stdout) printf("Progress: 0...");
		int i;
		for (i = 0; i < file->total_chunks; i++) {
			//printf("\t[%d,%d] %d\n", file->chunks[i].row, file->chunks[i].col, file->chunks[i].order);//
			
			chunkfile = (char *) realloc(chunkfile, (strlen(DISK_PATH) + strlen(DISK_ARRAY[file->chunks[i].col]) + toDigit(file->chunks[i].row) + 3) * sizeof(char));
			sprintf(chunkfile, "%s/%s/%d", DISK_PATH, DISK_ARRAY[file->chunks[i].col],file->chunks[i].row);
			
			FILE *fp = fopen(chunkfile, "rb");
			if (fp != NULL) {
				//printf("Opened %s...\n", chunkfile);
			} else { return; }

			long file_size = fs_getFileSize(chunkfile);

			bytes_written = 0;

			while (bytes_written < file_size) {
				bytes_read = fread(buffer, 1, BUFFER_SIZE, fp);
				fwrite(buffer, 1, bytes_read, file_out);					 // write bytes from buffer to the current slice
				if (to_stdout) fflush(stdout);
				bytes_written += BUFFER_SIZE;
			}
			fclose(fp);
			
			if (!to_stdout) printf("%d...", ((i+1)*100)/file->total_chunks);
		}

		if (!to_stdout) printf("done!\n");
		free(file);
		free(chunkfile);
		fclose(file_out);
		
		if (!to_stdout) printf("\nFile transaction completed successfully.\n");
	} else {
		printf("File not found.\n");
	}
}
