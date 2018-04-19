#ifndef PARAMETERSERIALIZATION_H
#define PARAMETERSERIALIZATION_H

enum parameter_type
{
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

struct parameter_update_entry
{
    uint32_t type;
    uint32_t ID;
    parameter_value value;
};


#endif // PARAMETERSERIALIZATION_H
