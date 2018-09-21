#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include <stdint.h>

#pragma pack(push, 1)

enum parameter_type
{
	parametertype_bool = 0, //NOTE: Don't change the values of these. They are used as indexes in some lookups in sqlhandler.cpp
    parametertype_double = 1,
    parametertype_uint = 2,
    parametertype_ptime = 3,
	parametertype_notsupported,
};

struct parameter_value
{
    union
    {
        double val_double;
        uint64_t val_uint;
        uint64_t val_bool;
        int64_t val_ptime;
    };
};

struct parameter_serial_entry
{
    uint32_t type;
    uint32_t ID;
    parameter_value value;
};

struct parameter_min_max_val_serial_entry
{
    uint32_t type;
    uint32_t ID;
    parameter_value min;
    parameter_value max;
    parameter_value value;
};

struct structure_serial_entry
{
    uint32_t parentID;
    uint32_t childID;
    uint32_t childNameLen;
	uint32_t unitLen;
};

#pragma pack(pop)

#define EXPORT_STRUCTURE_COMMAND "export_structure"
#define EXPORT_VALUES_COMMAND "export_values"

#endif // SERIALIZATION_H
