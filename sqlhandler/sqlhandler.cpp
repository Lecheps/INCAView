

//This program is for running on the google cloud virtual machine. It will recieve and handle requests
//from INCAView that are targeted at the working database.


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "sqlite3.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef int64_t s64;
typedef double f64;

#include "parameterserialization.h"

void print_parameter_value(char *buffer, parameter_update_entry *entry)
{
	parameter_type type = (parameter_type)entry->type;
	switch(type)
	{
		case parametertype_double:
		{
			sprintf(buffer, "%f", entry->value.val_double);
		} break;
		
		case parametertype_uint:
		{
			sprintf(buffer, "%u", entry->value.val_uint);
		} break;
		
		case parametertype_bool:
		{
			sprintf(buffer, "%d", entry->value.val_bool);
		} break;
		
		case parametertype_ptime:
		{
			sprintf(buffer, "%u" PRIu64 "", entry->value.val_ptime);
		} break;
	}
}

void handle_parameter_write_request(sqlite3 *db, const char *infilename)
{
	FILE *file = fopen(infilename, "r");
	if(!file)
	{
		perror("fopen");
	}
	u64 numparameters;
	fread(&numparameters, sizeof(u64), 1, file);
	for(int i = 0; i < numparameters; ++i)
	{
		parameter_update_entry entry;
		fread(&entry, sizeof(parameter_update_entry), 1, file);
		
		char valuestring[64];
		print_parameter_value(valuestring, &entry);
		
		char sqlcommand[512];
		sprintf(sqlcommand, "UPDATE ParameterValues SET value = %s WHERE ID = %u;", valuestring, entry.ID);
		fprintf(stdout, sqlcommand);
		
		char *errmsg;
		int rc = sqlite3_exec(db, sqlcommand, 0, 0, &errmsg);
		if(rc != SQLITE_OK)
		{
			fprintf(stderr, "SQL error: %s\n", errmsg);
			sqlite3_free(errmsg);
		}
	}
	
	fclose(file);
}

void write_parameter_update_test_file(char *filename)
{
	parameter_update_entry entries[3] =
	{
		(u32)parametertype_uint, (u32)3, (u64)4,
		(u32)parametertype_double, (u32)4, 1.0,
		(u32)parametertype_double, (u32)7, 2.5,
	};
	
	u64 count = 3;
	FILE *file = fopen(filename, "w");
	fwrite(&count, sizeof(u64), 1, file);
	fwrite(entries, sizeof(parameter_update_entry)*count, 1, file);
	
	fclose(file);
}



struct callback_data
{
	FILE *outfile;
	u64 count;
};

static int results_callback(void *data, int argc, char **argv, char **colname)
{
	callback_data *outdata = (callback_data *)data;
	++outdata->count;
	
	f64 value;
	int num = sscanf(argv[0], "%lf", &value);
	if(num != 1) value = 0.0;
	
	size_t count = fwrite(&value, sizeof(f64), 1, outdata->outfile);
	assert(count == 1);
	return 0;
}

static int count_callback(void *data, int argc, char **argv, char **colname)
{
	u64 *count = (u64 *)data;
	*count = (u64)atoi(argv[0]); //TODO: Do we need to support larger numbers than what is handled by atoi?
	return 0;
}

void handle_results_request(sqlite3 *db, u32 numrequests, u32* requested_ids, const char *outfilename)
{
	FILE *file = fopen(outfilename, "w");
	if(!file)
	{
		perror("fopen");
	}
	u64 numrequests64 = (u64)numrequests;
	fwrite(&numrequests64, sizeof(u64), 1, file);
	
	for(u32 i = 0; i < numrequests; ++i)
	{
		char sqlcount[256];
		sprintf(sqlcount, "SELECT count(*) FROM Results WHERE ID=%d;", requested_ids[i]);
		char *errmsg = 0;
		u64 count;
		int rc = sqlite3_exec(db, sqlcount, count_callback, (void *)&count, &errmsg);
		if( rc != SQLITE_OK )
		{
			fprintf(stderr, "SQL error: %s\n", errmsg);
			sqlite3_free(errmsg);
			fclose(file);
			return;
		}
		fwrite(&count, sizeof(u64), 1, file);
		
		char sqlcommand[256];
		sprintf(sqlcommand, "SELECT value FROM Results WHERE ID=%d;", requested_ids[i]);
		
		callback_data outdata;
		outdata.outfile = file;
		outdata.count = 0;
		rc = sqlite3_exec(db, sqlcommand, results_callback, (void *)&outdata, &errmsg);
		
		if( rc != SQLITE_OK )
		{
			fprintf(stderr, "SQL error: %s\n", errmsg);
			sqlite3_free(errmsg);
			fclose(file);
			return;
		}
		assert(count == outdata.count);
	}
	
	fclose(file);
}

void test_results_file(const char *filename)
{
	fprintf(stdout, "Testing the results file:\n");
	FILE *file = fopen(filename, "r");
	u64 numresults;
	fread(&numresults, sizeof(u64), 1, file);
	fprintf(stdout, "Number of result sets: %I64u\n", numresults);
	
	if(numresults < 50) // sanity check
	{
		for(u32 i = 0; i < numresults; ++i)
		{
			u64 count;
			fread(&count, sizeof(u64), 1, file);
			fprintf(stdout, "Result with count %I64u\n", count);
			
			f64 *data = (f64 *)malloc(count*sizeof(f64));
			fread(data, count*sizeof(f64), 1, file);
			free(data);
		}
	}
}



int main(int argc, char *argv[])
{
	assert(sizeof(f64)==8);
		
	if(argc > 3)
	{
		char *dbname = argv[2];
		char *filename = argv[3];
		sqlite3 *db;
		int rc = sqlite3_open(dbname, &db);

		if(rc)
		{
			fprintf(stderr, "Can't open database!\n");
		}
		else
		{
			fprintf(stdout, "Succeeded in opening database!\n");
			if(strcmp(argv[1], "results") == 0)
			{
				u32 numrequests = argc - 4;
				if(numrequests > 0)
				{
					u32 *requested_ids = (u32 *)malloc(numrequests*sizeof(u32));
					for(u32 i = 0; i < numrequests; ++i)
					{
						
						u32 ID = (u32)atoi(argv[4+i]);
						//TODO: check if format was correct
						requested_ids[i] = ID;
					}
					handle_results_request(db, numrequests, requested_ids, filename);
					
					free(requested_ids);
					
					//test_results_file(filename);
				}
			}
			else if(strcmp(argv[1], "write_parameters") == 0)
			{
				write_parameter_update_test_file(filename);
				
				handle_parameter_write_request(db, filename);
			}
		}	
			
		sqlite3_close(db);
	}
	return 0;
}