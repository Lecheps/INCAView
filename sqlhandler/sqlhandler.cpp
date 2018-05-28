

//This program is for running on the google cloud virtual machine. It will recieve and handle requests
//from INCAView that are targeted at the working database.

/*
NOTE: This program should always print exactly one of the following:
	"ERROR: <specification of error>" if an error occured
	"SUCCESS: <whatever you want>" otherwise.
They have to be printed to the stdout, because INCAView will only be able to listen to one channel at a time over the ssh connection.
*/

/*
Stuff left to do:
- Error reporting in case of errors in fread, fwrite, malloc.
- Error checking of argv input? (Should not be needed since this program is only called by INCAView).
- ...
*/

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


void print_parameter_value(char *buffer, parameter_serial_entry *entry)
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
			sprintf(buffer, "%d", entry->value.val_ptime);
		} break;
	}
}

/*
parameter_value parse_parameter_value(char *valstr, parameter_type type)
{
	parameter_value result;
	switch(type)
	{
		case parametertype_double:
		{
			double value;
			if(sscanf(valstr, "%lf", &value)!= 1) value = 0.0;
			result.val_double = value;
		} break;
		
		//TODO: Properly support the below formats
		//NOTE: Currently the internal format of all of these in the database is double..
		// This may be changed in the future..
		case parametertype_uint:
		{
			double value;
			if(sscanf(valstr, "%lf", &value)!= 1) value = 0.0;
			result.val_uint = (u64)value;
		} break;
		
		case parametertype_bool:
		{
			double value;
			if(sscanf(valstr, "%lf", &value)!= 1) value = 0.0;
			result.val_bool = (u64)value;
		} break;
		
		case parametertype_ptime:
		{
			double value;
			if(sscanf(valstr, "%lf", &value)!= 1) value = 0.0;
			result.val_ptime = (s64)value;
		} break;
		
		default:
		{
			result.val_uint = 0;
		} break;
	}
	
	return result;
}*/

parameter_type parse_parameter_type(const char *typestr)
{
	parameter_type type = parametertype_notsupported;
	
	if(strcmp(typestr, "DOUBLE") == 0) type = parametertype_double;
	else if(strcmp(typestr, "UINT") == 0) type = parametertype_uint;
	else if(strcmp(typestr, "BOOL") == 0) type = parametertype_bool;
	else if(strcmp(typestr, "PTIME") == 0) type = parametertype_ptime;
	
	return type;
}


static int export_structure_callback(void *data, int argc, char **argv, char **colname)
{
	FILE *file = (FILE *)data;

	assert(argc == 3);
	structure_serial_entry outdata;
	outdata.parentID = atoi(argv[0]);
	outdata.childID = atoi(argv[1]);
	char *childName = argv[2];
	outdata.childNameLen = strlen(childName);
	
	//fprintf(stdout, "%u %u %u %s\n", outdata.parentID, outdata.childID, outdata.childNameLen, childName);
	
	fwrite(&outdata, sizeof(outdata), 1, file);
	fwrite(childName, outdata.childNameLen, 1, file);
	return 0;
}

bool export_parameter_structure(sqlite3 *db, FILE *file)
{
	const char *sqlcommand = "SELECT parent.ID as parentID, child.ID, child.Name "
                              "FROM ParameterStructure as parent, ParameterStructure as child "
                              "WHERE child.lft > parent.lft "
                              "AND child.rgt < parent.rgt "
                              "AND child.dpt = parent.dpt + 1 "
                              "UNION "
                              "SELECT 0 as parentID, child.ID, child.Name " 
                              "FROM ParameterStructure as child "
                              "WHERE child.dpt = 0 "
							  "ORDER BY child.ID";
	char *errmsg = 0;
	int rc = sqlite3_exec(db, sqlcommand, export_structure_callback, (void *)file, &errmsg);
	
	if( rc != SQLITE_OK )
	{
		fprintf(stdout, "ERROR: SQL error: %s\n", errmsg);
		sqlite3_free(errmsg);
		fclose(file);
		return false;
	}
	
	return true;
}

bool export_results_structure(sqlite3 *db, FILE *file)
{
	const char *sqlcommand = "SELECT parent.ID AS parentID, child.ID, child.Name "
							 "FROM ResultsStructure AS parent, ResultsStructure AS child "
							 "WHERE child.lft > parent.lft "
							 "AND child.rgt < parent.rgt "
							 "AND child.dpt = parent.dpt + 1 "
							 "UNION "
							 "SELECT 0 as parentID, child.ID, child.Name "
							 "FROM ResultsStructure as child "
							 "WHERE child.dpt = 0 "
							 "ORDER BY child.ID";
	char *errmsg = 0;
	int rc = sqlite3_exec(db, sqlcommand, export_structure_callback, (void *)file, &errmsg);
	
	if( rc != SQLITE_OK )
	{
		fprintf(stdout, "ERROR: SQL error: %s\n", errmsg);
		sqlite3_free(errmsg);
		fclose(file);
		return false;
	}
	
	return true;
}


bool export_parameter_values_min_max(sqlite3 *db, FILE *file)
{
	const char *value_tables[4] =
	{
		"bool",
		"double",
		"int",
		"ptime",
	};
	
	parameter_type types[4] =
	{
		parametertype_bool,
		parametertype_double,
		parametertype_uint,
		parametertype_ptime,
	};
	
	for(int i = 0; i < 4; ++i)
	{
		char commandbuf[512];
		sprintf(commandbuf,
			"SELECT "
			 "ParameterStructure.ID, ParameterStructure.type, ParameterValues_%s.minimum, ParameterValues_%s.maximum, ParameterValues_%s.value "
			 "FROM ParameterStructure INNER JOIN ParameterValues_%s "
			 "ON ParameterStructure.ID = ParameterValues_%s.ID; ",
			 value_tables[i], value_tables[i], value_tables[i], value_tables[i], value_tables[i]);
		sqlite3_stmt *stmt;
		int rc = sqlite3_prepare_v2(db, commandbuf, -1, &stmt, 0);
		if(rc != SQLITE_OK)
		{
			fprintf(stdout, "ERROR; SQL, could not prepare statement %s\n", commandbuf);
			fclose(file);
			return false;
		}
		rc = sqlite3_step(stmt);
		while(rc == SQLITE_ROW)
		{
			parameter_min_max_val_serial_entry entry;
			entry.ID = sqlite3_column_int(stmt, 0);
			const char *typetxt = (const char *)sqlite3_column_text(stmt, 1); // NOTE: The cast should be ok since we only use ASCII
			entry.type = parse_parameter_type(typetxt);
			if(entry.type != types[i])
			{
				fprintf(stdout, "ERROR: Database: Parameter with ID %d is registered as having type %s, but is in the table for %s.\n",
					entry.ID, typetxt, value_tables[i]);
				fclose(file);
				return false;
			}
			switch(types[i])
			{
				case parametertype_bool :
				{
					entry.min.val_bool = sqlite3_column_int(stmt, 2);
					entry.max.val_bool = sqlite3_column_int(stmt, 3);
					entry.value.val_bool = sqlite3_column_int(stmt, 4);
				} break;
				
				case parametertype_double :
				{
					entry.min.val_double = sqlite3_column_double(stmt, 2);
					entry.max.val_double = sqlite3_column_double(stmt, 3);
					entry.value.val_double = sqlite3_column_double(stmt, 4);
				} break;
				
				case parametertype_uint :
				{
					entry.min.val_uint = sqlite3_column_int(stmt, 2);
					entry.max.val_uint = sqlite3_column_int(stmt, 3);
					entry.value.val_uint = sqlite3_column_int(stmt, 4);
				} break;
				
				case parametertype_ptime :
				{
					entry.min.val_ptime = sqlite3_column_int64(stmt, 2);
					entry.max.val_ptime = sqlite3_column_int64(stmt, 3);
					entry.value.val_ptime = sqlite3_column_int64(stmt, 4);
				} break;
			}
			
			fwrite(&entry, sizeof(parameter_min_max_val_serial_entry), 1, file);
			
			rc = sqlite3_step(stmt);
		}
		if(rc == SQLITE_ERROR)
		{
			fprintf(stdout, "ERROR: SQL Error in stepping through statement.\n");
			return false;
		}
		else if(rc != SQLITE_DONE)
		{
			fprintf(stdout, "ERROR: SQL Unknown mishap in stepping through statement.\n");
			return false;
		}
	}
	
	return true;
}



bool import_parameter_values(sqlite3 *db, FILE *file)
{
	u64 numparameters;
	fread(&numparameters, sizeof(u64), 1, file);
	for(int i = 0; i < numparameters; ++i)
	{
		parameter_serial_entry entry;
		fread(&entry, sizeof(parameter_serial_entry), 1, file);
		
		char valuestring[64];
		
		//TODO: instead of setting these using a string, we could bind the values directly to the query.
		print_parameter_value(valuestring, &entry);
		
		char sqlcommand[512];
		switch(entry.type)
		{
			case parametertype_double:
			{
				sprintf(sqlcommand, "UPDATE ParameterValues_double SET value = %s WHERE ID = %u;", valuestring, entry.ID);
			} break;
			
			case parametertype_bool:
			{
				sprintf(sqlcommand, "UPDATE ParameterValues_bool SET value = %s WHERE ID = %u;", valuestring, entry.ID);
			} break;
			
			case parametertype_uint:
			{
				sprintf(sqlcommand, "UPDATE ParameterValues_int SET value = %s WHERE ID = %u;", valuestring, entry.ID);
			} break;
			
			case parametertype_ptime:
			{
				sprintf(sqlcommand, "UPDATE ParameterValues_ptime SET value = %s WHERE ID = %u;", valuestring, entry.ID);
			} break;
			
			default:
			{
				fprintf(stdout, "ERROR: Received unknown parameter type for parameter with ID %d", entry.ID);
				return false;
			} break;
		}
		//fprintf(stdout, sqlcommand);
		
		char *errmsg;
		int rc = sqlite3_exec(db, sqlcommand, 0, 0, &errmsg);
		if(rc != SQLITE_OK)
		{
			fprintf(stdout, "ERROR: SQL error: %s\n", errmsg);
			sqlite3_free(errmsg);
			fclose(file);
			return false;
		}
	}
	
	return true;
}

struct export_result_values_callback_data
{
	FILE *outfile;
	u64 count;
};

static int export_result_values_callback(void *data, int argc, char **argv, char **colname)
{
	export_result_values_callback_data *outdata = (export_result_values_callback_data *)data;
	++outdata->count;
	
	f64 value;
	int num = sscanf(argv[0], "%lf", &value);
	if(num != 1) value = 0.0;
	
	size_t count = fwrite(&value, sizeof(f64), 1, outdata->outfile);
	assert(count == 1);
	return 0;
}

static int export_result_values_count_callback(void *data, int argc, char **argv, char **colname)
{
	u64 *count = (u64 *)data;
	*count = (u64)atoi(argv[0]); //TODO: Do we need to support larger numbers than what is handled by atoi?
	return 0;
}

bool export_result_values(sqlite3 *db, u32 numrequests, u32* requested_ids, FILE *file)
{
	u64 numrequests64 = (u64)numrequests;
	fwrite(&numrequests64, sizeof(u64), 1, file);
	
	for(u32 i = 0; i < numrequests; ++i)
	{
		char sqlcount[256];
		sprintf(sqlcount, "SELECT count(*) FROM Results WHERE ID=%d;", requested_ids[i]);
		char *errmsg = 0;
		u64 count;
		int rc = sqlite3_exec(db, sqlcount, export_result_values_count_callback, (void *)&count, &errmsg);
		if( rc != SQLITE_OK )
		{
			fprintf(stdout, "ERROR: SQL error: %s\n", errmsg);
			sqlite3_free(errmsg);
			fclose(file);
			return false;
		}
		fwrite(&count, sizeof(u64), 1, file);
		
		char sqlcommand[256];
		sprintf(sqlcommand, "SELECT value FROM Results WHERE ID=%d;", requested_ids[i]);
		
		export_result_values_callback_data outdata;
		outdata.outfile = file;
		outdata.count = 0;
		rc = sqlite3_exec(db, sqlcommand, export_result_values_callback, (void *)&outdata, &errmsg);
		
		if( rc != SQLITE_OK )
		{
			fprintf(stdout, "ERROR: SQL error: %s\n", errmsg);
			sqlite3_free(errmsg);
			fclose(file);
			return false;
		}
		assert(count == outdata.count);
	}
	
	return true;
}


void test_result_values_file(const char *filename);
void test_structure_file(const char *filename);
void write_parameter_import_test_file(const char *filename);

int main(int argc, char *argv[])
{
	assert(sizeof(f64)==8);
		
	if(argc > 3)
	{
		char *dbname = argv[2];
		char *filename = argv[3];
		sqlite3 *db;
		FILE *file;
		if(strcmp(argv[1], IMPORT_PARAMETER_VALUES_COMMAND) == 0)
		{
			file = fopen(filename, "r");
		}
		else
		{
			file = fopen(filename, "w");
		}
		
		if(!file)
		{
			fprintf(stdout, "ERROR: Unable to open file %s", filename);
			return 0;
		}
		
		int rc = sqlite3_open_v2(dbname, &db, SQLITE_OPEN_READWRITE, 0);
		
		if(rc != SQLITE_OK)
		{
			fprintf(stdout, "ERROR: Unable to open database %s: %s\n", dbname, sqlite3_errmsg(db));
			return 0;
		}

		bool success = false;
		if(strcmp(argv[1], EXPORT_RESULT_VALUES_COMMAND) == 0)
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
				success = export_result_values(db, numrequests, requested_ids, file);
				
				free(requested_ids);
				
				//test_result_values_file(filename);
			}
		}
		else if(strcmp(argv[1], IMPORT_PARAMETER_VALUES_COMMAND) == 0)
		{
			//write_parameter_import_test_file((filename);
			
			success = import_parameter_values(db, file);
		}
		else if(strcmp(argv[1], EXPORT_RESULTS_STRUCTURE_COMMAND) == 0)
		{
			success = export_results_structure(db, file);
			
			//test_structure_file(filename);
		}
		else if(strcmp(argv[1], EXPORT_PARAMETER_STRUCTURE_COMMAND) == 0)
		{
			success = export_parameter_structure(db, file);
			//test_structure_file(filename);
		}
		else if(strcmp(argv[1], EXPORT_PARAMETER_VALUES_MIN_MAX_COMMAND) == 0)
		{
			success = export_parameter_values_min_max(db, file);
		}
		else
		{
			fprintf(stdout, "ERROR: Unexpected command: %s\n", argv[1]);
		}
		
		if(success)
		{
			fprintf(stdout, "SUCCESS: Successfully executed command: %s", argv[1]);
		}
			
		sqlite3_close(db);
		fclose(file);
	}
	else
	{
		fprintf(stdout, "ERROR: To few arguments to sqlhandler. Got %d arguments, expected at least 4", argc);
	}
	return 0;
}





//////////// TESTS /////////////////////

void test_result_values_file(const char *filename)
{
	fprintf(stdout, "Testing the result values file:\n");
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
	fclose(file);
}

//NOTE! This test routine has a bug in some cases!
void test_structure_file(const char *filename)
{
	fprintf(stdout, "Testing the structure file:\n");
	FILE *file = fopen(filename, "r");
	while(!feof(file))
	{
		structure_serial_entry entry;
		fread(&entry, sizeof(structure_serial_entry), 1, file);
		assert(entry.childNameLen < 512);
		char childNameBuf[512];
		fread(childNameBuf, entry.childNameLen, 1, file);
		childNameBuf[entry.childNameLen] = 0;
		fprintf(stdout, "Entry: parentID: %u, childID: %u, childNameLen: %u, childName: %s\n", entry.parentID, entry.childID, entry.childNameLen, childNameBuf);
	}
	fclose(file);
}


void write_parameter_import_test_file(const char *filename)
{
	parameter_serial_entry entries[3] =
	{
		(u32)parametertype_uint, (u32)3, (u64)4,
		(u32)parametertype_double, (u32)4, 1.0,
		(u32)parametertype_double, (u32)7, 2.5,
	};
	
	u64 count = 3;
	FILE *file = fopen(filename, "w");
	fwrite(&count, sizeof(u64), 1, file);
	fwrite(entries, sizeof(parameter_serial_entry)*count, 1, file);
	
	fclose(file);
}