#include "common.h"

namespace MultivariateSplines
{

double checked_strtod(const char* _Str, char** _Eptr) {

    double _Ret;
    char* _EptrTmp;

    errno = 0;

    _Ret = std::strtod(_Str, &_EptrTmp);

    if(_EptrTmp == _Str)
    {
        throw std::invalid_argument("strtod");
    }
    else if(errno == ERANGE)
    {
        throw std::out_of_range("strtod");
    }
    else
    {
        if(_Eptr != nullptr)
        {
            *_Eptr = _EptrTmp;
        }

        return _Ret;
    }
}

int checked_strtol(const char* _Str, char** _Eptr, size_t _Base) {

    long _Ret;
    char* _EptrTmp;

    errno = 0;

    _Ret = std::strtol(_Str, &_EptrTmp, _Base);

    if(_EptrTmp == _Str)
    {
        throw std::invalid_argument("strtol");
    }
    else if(errno == ERANGE ||
            (_Ret < std::numeric_limits<int>::min() || _Ret > std::numeric_limits<int>::max()))
    {
        throw std::out_of_range("strtol");
    }
    else
    {
        if(_Eptr != nullptr)
        {
            *_Eptr = _EptrTmp;
        }

        return _Ret;
    }
}
}
