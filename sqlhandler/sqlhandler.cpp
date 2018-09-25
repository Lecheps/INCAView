

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
#include <limits>
#include "sqlite3.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef int64_t s64;
typedef double f64;

#include "serialization.h"


bool export_structure(sqlite3 *db, FILE *file, const char *table)
{	
	char sqlcommand[1024];
	sprintf(sqlcommand,
				"SELECT parent.ID AS parentID, child.ID, child.name, child.unit "
				"FROM %s AS parent, %s AS child "
				"WHERE child.lft > parent.lft "
				"AND child.rgt < parent.rgt "
				"AND child.dpt = parent.dpt + 1 "
				"UNION "
				"SELECT 0 as parentID, child.ID, child.name, child.unit "
				"FROM %s as child "
				"WHERE child.dpt = 0 "
				"ORDER BY child.ID",
			table, table, table
	);
	
	sqlite3_stmt *statement;
	int rc = sqlite3_prepare_v2(db, sqlcommand, -1, &statement, 0);
	
	if( rc != SQLITE_OK )
	{
		fprintf(stdout, "ERROR: SQL error: %s\n", sqlite3_errmsg(db));
		return false;
	}
	
	while((rc = sqlite3_step(statement)) != SQLITE_DONE)
	{
		if(rc == SQLITE_ERROR)
		{
			fprintf(stdout, "ERROR: SQL error: %s\n", sqlite3_errmsg(db));
			return false;
		}
		
		structure_serial_entry outdata = {};
		outdata.parentID = sqlite3_column_int(statement, 0);
		outdata.childID  = sqlite3_column_int(statement, 1);
		const char *childName = (const char *)sqlite3_column_text(statement, 2);
		outdata.childNameLen = strlen(childName);
		const char *unitName = (const char *)sqlite3_column_text(statement, 3);
		if(unitName) outdata.unitLen = strlen(unitName);
		
		fwrite(&outdata, sizeof(outdata), 1, file);
		fwrite(childName, 1, outdata.childNameLen, file);
		if(unitName) fwrite(unitName, 1, outdata.unitLen, file);
		
		//fprintf(stdout, "%u %u %u %u %s %s\n", outdata.parentID, outdata.childID, outdata.childNameLen, outdata.unitLen, childName, unitName);
	}
	
	sqlite3_finalize(statement);
	
	return true;
}

static bool export_values(sqlite3 *db, u32 numrequests, u32* requested_ids, FILE *file, const char *table)
{
	u64 numrequests64 = (u64)numrequests;
	fwrite(&numrequests64, sizeof(u64), 1, file);
	
	//NOTE: Write out the start date of the dataset only
	//NOTE: This is kind of a hack since it is only useful if the recipient knows the timestep. Also will not work if different timeseries have different start dates, 
	// but at least for now that is not allowed.
	char sqldate[256];
	sprintf(sqldate, "SELECT date from %s WHERE ID=%d LIMIT 1", table, requested_ids[0]);
	sqlite3_stmt *statement;
	int rc = sqlite3_prepare_v2(db, sqldate, -1, &statement, 0);
	if( rc != SQLITE_OK )
	{
		fprintf(stdout, "ERROR: SQL error: %s\n", sqlite3_errmsg(db));
		return false;
	}
	rc = sqlite3_step(statement);
	if(rc == SQLITE_ERROR)
	{
		fprintf(stdout, "ERROR: SQL error: %s\n", sqlite3_errmsg(db));
		return false;
	}
	s64 date = sqlite3_column_int64(statement, 0);
	
	fwrite(&date, sizeof(s64), 1, file);
	
	sqlite3_finalize(statement);
	
	for(u32 i = 0; i < numrequests; ++i)
	{
		char sqlcount[256];
		sprintf(sqlcount, "SELECT count(*) FROM %s WHERE ID=%d;", table, requested_ids[i]);
		
		rc = sqlite3_prepare_v2(db, sqlcount, -1, &statement, 0);
		if( rc != SQLITE_OK )
		{
			fprintf(stdout, "ERROR: SQL error: %s\n", sqlite3_errmsg(db));
			return false;
		}
		
		u64 count;
		
		rc = sqlite3_step(statement);
		if(rc == SQLITE_ERROR)
		{
			fprintf(stdout, "ERROR: SQL error: %s\n", sqlite3_errmsg(db));
			return false;
		}
		
		count = (u64)sqlite3_column_int(statement, 0);
		fwrite(&count, sizeof(u64), 1, file);
		
		sqlite3_finalize(statement);
		
		char sqlcommand[256];
		sprintf(sqlcommand, "SELECT value FROM %s WHERE ID=%d;", table, requested_ids[i]);
		
		rc = sqlite3_prepare_v2(db, sqlcommand, -1, &statement, 0);
		if( rc != SQLITE_OK )
		{
			fprintf(stdout, "ERROR: SQL error: %s\n", sqlite3_errmsg(db));
			return false;
		}
		
		while((rc = sqlite3_step(statement)) != SQLITE_DONE)
		{
			if(rc == SQLITE_ERROR)
			{
				fprintf(stdout, "ERROR: SQL error: %s\n", sqlite3_errmsg(db));
				return false;
			}
			
			double value;
			if(sqlite3_column_type(statement, 0) == SQLITE_NULL)
			{
				value = std::numeric_limits<double>::quiet_NaN();
			}
			else
			{
				value = sqlite3_column_double(statement, 0);
			}
			fwrite(&value, sizeof(double), 1, file);
		}
		
		sqlite3_finalize(statement);
	}
	
	return true;
}


int main(int argc, char *argv[])
{
	assert(sizeof(f64)==8);
		
	if(argc >= 5)
	{
		//NOTE: exename is argv[0];
		const char *command  = argv[1];
		const char *dbname   = argv[2];
		const char *filename = argv[3];
		const char *table    = argv[4];
		sqlite3 *db;
		FILE *file = fopen(filename, "w");

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
		
		if(strcmp(command, EXPORT_VALUES_COMMAND) == 0)
		{
			u32 numrequests = argc - 5;
			if(numrequests > 0)
			{
				u32 *requested_ids = (u32 *)malloc(numrequests*sizeof(u32));
				for(u32 i = 0; i < numrequests; ++i)
				{	
					u32 ID = (u32)atoi(argv[5+i]);
					//TODO: check if format was correct
					requested_ids[i] = ID;
				}
				success = export_values(db, numrequests, requested_ids, file, table);
				
				free(requested_ids);
				
				//test_result_values_file(filename);
			}
		}
		else if(strcmp(command, EXPORT_STRUCTURE_COMMAND) == 0)
		{
			success = export_structure(db, file, table);
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
/*
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
*/