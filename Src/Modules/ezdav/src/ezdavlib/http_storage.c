#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http.h"
#include "http_storage.h"

extern http_allocator _http_allocator;
extern void* _http_allocator_user_data;

void http_destroy_generic_storage(HTTP_STORAGE **storage)
{
	if(storage != NULL && *storage != NULL)
	{
		(*storage)->destroy(*storage);
		_http_allocator(_http_allocator_user_data, *storage, 0);
		*storage = NULL;
	}
}

int
http_write_memory_storage(HTTP_MEMORY_STORAGE *storage, const char *data, int size)
{
	int new_content_size;
	int new_content_buffer_size;
	char *new_content_buffer;
	if(storage == NULL || (data == NULL && size != 0))
	{
		return HT_INVALID_ARGUMENT;
	}
	new_content_size = storage->content_size + size;
	if(new_content_size > storage->content_buffer_size)
	{
		new_content_buffer_size = storage->content_buffer_size;
		if(new_content_buffer_size == 0)
		{
			new_content_buffer_size = 4096;
		}
		while(new_content_buffer_size < new_content_size)
		{
			new_content_buffer_size += new_content_buffer_size / 4;
		}
		new_content_buffer = (char *) _http_allocator(_http_allocator_user_data, storage->content, new_content_buffer_size);
		if(new_content_buffer == NULL)
		{
			return HT_MEMORY_ERROR;
		}
		storage->content = new_content_buffer;
		storage->content_buffer_size = new_content_buffer_size;
	}
	memcpy(storage->content + storage->content_size, data, size);
	storage->content_size = new_content_size;
	return HT_OK;
}

int
http_seek_memory_storage(HTTP_MEMORY_STORAGE *storage, int location)
{
	if(location >= storage->content_size)
	{
		return HT_ILLEGAL_OPERATION;
	}
	storage->content_index = location;
	return HT_OK;
}

int
http_read_memory_storage(HTTP_MEMORY_STORAGE *storage, char *buffer, int buffer_size, int *read_count)
{
	if(storage->content_index + buffer_size <= storage->content_size)
	{
		*read_count = buffer_size;
	}
	else
	{
		*read_count = storage->content_size - storage->content_index;
	}
	memcpy(buffer, storage->content + storage->content_index, *read_count);
	storage->content_index += *read_count;
	return HT_OK;
}

int
http_get_memory_storage_size(HTTP_MEMORY_STORAGE *storage, int *size)
{
	*size = storage->content_size;
	return HT_OK;
}

int 
http_close_memory_storage(HTTP_MEMORY_STORAGE *storage)
{
	/* does nothing */
	return HT_OK;
}

void
http_destroy_memory_storage(HTTP_MEMORY_STORAGE *storage)
{
	_http_allocator(_http_allocator_user_data, storage->content, 0);
}

int
http_create_memory_storage(HTTP_MEMORY_STORAGE **storage)
{
	HTTP_MEMORY_STORAGE *new_storage;
	if(storage == NULL)
	{
		return HT_INVALID_ARGUMENT;
	}
	new_storage = (HTTP_MEMORY_STORAGE *) _http_allocator(_http_allocator_user_data, 0, sizeof(HTTP_MEMORY_STORAGE));
	if(new_storage == NULL)
	{
		return HT_MEMORY_ERROR;
	}
	memset(new_storage, 0, sizeof(HTTP_MEMORY_STORAGE));
	new_storage->functions.write = (HTTP_STORAGE_WRITE) http_write_memory_storage;
	new_storage->functions.read = (HTTP_STORAGE_READ) http_read_memory_storage;
	new_storage->functions.seek = (HTTP_STORAGE_SEEK) http_seek_memory_storage;
	new_storage->functions.getsize = (HTTP_STORAGE_GETSIZE) http_get_memory_storage_size;
	new_storage->functions.close = (HTTP_STORAGE_CLOSE) http_close_memory_storage;
	new_storage->functions.destroy = (HTTP_STORAGE_DESTROY) http_destroy_memory_storage;
	*storage = new_storage;
	return HT_OK;
}

int
http_write_file_storage(HTTP_FILE_STORAGE *storage, const char *data, int size)
{
	int write_count;
	if(storage == NULL || (data == NULL && size != 0))
	{
		return HT_INVALID_ARGUMENT;
	}
	write_count = fwrite(data, sizeof(char), size, storage->file);
	storage->file_size += write_count;
	if(write_count != size)
	{
		return HT_IO_ERROR;
	}
	return HT_OK;
}

int
http_seek_file_storage(HTTP_FILE_STORAGE *storage, int location)
{
	if(location >= storage->file_size)
	{
		return HT_ILLEGAL_OPERATION;
	}
	fseek(storage->file, location, SEEK_SET);
	return HT_OK;
}

int
http_read_file_storage(HTTP_FILE_STORAGE *storage, char *buffer, int buffer_size, int *read_count)
{
	*read_count = fread(buffer, sizeof(char), buffer_size, storage->file);
	if(*read_count != buffer_size)
	{
		if(ferror(storage->file))
		{
			return HT_IO_ERROR;
		}
	}
	return HT_OK;
}

int
http_get_file_storage_size(HTTP_FILE_STORAGE *storage, int *size)
{
	*size = storage->file_size;
	return HT_OK;
}

int 
http_close_file_storage(HTTP_FILE_STORAGE *storage)
{
	if(storage->file != NULL)
	{
		fclose(storage->file);
		storage->file = NULL;
	}
	return HT_OK;
}

void
http_destroy_file_storage(HTTP_FILE_STORAGE *storage)
{
	http_close_file_storage(storage);
}

int 
http_create_file_storage(HTTP_FILE_STORAGE **storage, const char *filename, const char *mode)
{
	HTTP_FILE_STORAGE *new_storage = NULL;
	int original_pos = 0;
	if(storage == NULL)
	{
		return HT_INVALID_ARGUMENT;
	}
	new_storage = (HTTP_FILE_STORAGE *) _http_allocator(_http_allocator_user_data, 0, sizeof(HTTP_FILE_STORAGE));
	if(new_storage == NULL)
	{
		return HT_MEMORY_ERROR;
	}
	memset(new_storage, 0, sizeof(HTTP_FILE_STORAGE));
	new_storage->file = fopen(filename, mode);
	if(new_storage->file == NULL)
	{
		http_destroy_file_storage(new_storage);
		return HT_MEMORY_ERROR;
	}
	original_pos = ftell(new_storage->file);
	fseek(new_storage->file, 0, SEEK_END);
	new_storage->file_size = ftell(new_storage->file);
	fseek(new_storage->file, original_pos, SEEK_SET);

	new_storage->functions.write = (HTTP_STORAGE_WRITE) http_write_file_storage;
	new_storage->functions.read = (HTTP_STORAGE_READ) http_read_file_storage;
	new_storage->functions.seek = (HTTP_STORAGE_SEEK) http_seek_file_storage;
	new_storage->functions.getsize = (HTTP_STORAGE_GETSIZE) http_get_file_storage_size;
	new_storage->functions.close = (HTTP_STORAGE_CLOSE) http_close_file_storage;
	new_storage->functions.destroy = (HTTP_STORAGE_DESTROY) http_destroy_file_storage;
	*storage = new_storage;
	return HT_OK;
}
