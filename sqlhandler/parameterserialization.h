#ifndef PARAMETERSERIALIZATION_H
#define PARAMETERSERIALIZATION_H


#pragma pack(push, 1)

enum parameter_type
{
    parametertype_notsupported,
    parametertype_double,
    parametertype_uint,
    parametertype_bool,
    parametertype_ptime,
};

struct parameter_value
{
    union
    {
        //NOTE: These should be the same bit size.
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
};

#pragma pack(pop)

#endif // PARAMETERSERIALIZATION_H
