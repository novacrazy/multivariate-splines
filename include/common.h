#ifndef MS_COMMON_H
#define MS_COMMON_H


#include <iostream>
#include <iomanip>

#include <vector>
#include <Eigen/Dense>
#include <Eigen/Sparse>

namespace MultivariateSplines
{

// Eigen vectors
typedef Eigen::VectorXd DenseVector;
typedef Eigen::SparseVector<double> SparseVector;

// Eigen matrices
typedef Eigen::MatrixXd DenseMatrix;
typedef Eigen::SparseMatrix<double> SparseMatrix; // declares a column-major sparse matrix type of double


template <typename T, size_t N = sizeof( T )>
struct endian_reverse {};

template <typename T>
struct endian_reverse<T, sizeof( uint16_t )>
{
    inline static T swap( const T& value )
    {
        uint16_t val = *reinterpret_cast<const uint16_t*>( &value );

        val = ( val << 8 ) | ( val >> 8 );

        return *reinterpret_cast<T*>( &val );
    }
};

template <typename T>
struct endian_reverse<T, sizeof( uint32_t )>
{
    inline static T swap( const T& value )
    {
        uint32_t val = *reinterpret_cast<const uint32_t*>( &value );

        val = ( ( val << 8 ) & 0xFF00FF00 ) | ( ( val >> 8 ) & 0xFF00FF );

        val = ( val << 16 ) | ( val >> 16 );

        return *reinterpret_cast<T*>( &val );
    }
};

template <typename T>
struct endian_reverse<T, sizeof( uint64_t )>
{
    inline static T swap( const T& value )
    {
        uint64_t val = *reinterpret_cast<const uint64_t*>( &value );

        val = ( ( val << 8 ) & 0xFF00FF00FF00FF00ULL ) | ( ( val >> 8 ) & 0x00FF00FF00FF00FFULL );
        val = ( ( val << 16 ) & 0xFFFF0000FFFF0000ULL ) | ( ( val >> 16 ) & 0x0000FFFF0000FFFFULL );
        val = ( val << 32 ) | ( val >> 32 );

        return *reinterpret_cast<T*>( &val );
    }
};

//Simple definition of checked strto* functions according to the implementations of sto* C++11 functions at:
//  http://msdn.microsoft.com/en-us/library/ee404775.aspx
//  http://msdn.microsoft.com/en-us/library/ee404860.aspx
//  https://gcc.gnu.org/svn/gcc/trunk/libstdc++-v3/include/bits/basic_string.h
//  https://gcc.gnu.org/svn/gcc/trunk/libstdc++-v3/include/ext/string_conversions.h

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

int checked_strtol(const char* _Str, char** _Eptr, size_t _Base = 10) {

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


} // namespace MultivariateSplines


#endif // MS_COMMON_H
