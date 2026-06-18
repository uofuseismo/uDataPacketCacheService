#ifndef PACK_UNPACK_HPP
#define PACK_UNPACK_HPP
#include <algorithm>
#include <bit>
#include <cstddef>
#include <string>
#include <vector>

namespace
{

template<typename T>
std::vector<T>
    unpack(const std::string &data, const uint32_t nSamples)
{
    const bool swapBytes
        = (std::endian::native == std::endian::little) ? false : true;
    constexpr auto dataTypeSize = sizeof(T);
    std::vector<T> result;
    if (nSamples < 1){return result;}
    if (static_cast<size_t> (nSamples)*dataTypeSize != data.size())
    {   
        throw std::invalid_argument("Unexpected data size");
    }   
    result.resize(nSamples);
    // Pack it up
    union CharacterValueUnion
    {   
        unsigned char cArray[dataTypeSize];
        T value;
    };  
    CharacterValueUnion cvUnion;
    if (!swapBytes)
    {   
        for (uint32_t i = 0; i < nSamples; ++i)
        {
            cvUnion.value = static_cast<unsigned char> (data[i]);
            auto i1 = i*dataTypeSize;
            auto i2 = i1 + dataTypeSize;
            std::copy(data.data() + i1, data.data() + i2, 
                      cvUnion.cArray);
            result[i] = cvUnion.value;
        }
    }
    else
    {   
        for (uint32_t i = 0; i < nSamples; ++i)
        {
            cvUnion.value = static_cast<unsigned char> (data[i]);
            auto i1 = i*dataTypeSize;
            auto i2 = i1 + dataTypeSize;
            std::reverse_copy(data.data() + i1, data.data() + i2, 
                              cvUnion.cArray);
            result[i] = cvUnion.value;
        }
    }
    return result;
}

template<typename T>
std::string pack(const T *data, const int nSamples, const bool swapBytes)
{
    constexpr auto dataTypeSize = sizeof(T);
    std::string result;
    if (nSamples < 1){return result;}
    result.resize(dataTypeSize*nSamples);
    // Pack it up
    union CharacterValueUnion
    {   
        char cArray[dataTypeSize]; // Unpack uses unsigned char so this pushes it
        T value;
    };  
    CharacterValueUnion cvUnion;
    if (!swapBytes)
    {   
        for (int i = 0; i < nSamples; ++i)
        {
            cvUnion.value = data[i];
            std::copy(cvUnion.cArray, cvUnion.cArray + dataTypeSize,
                      result.data() + dataTypeSize*i);
        }
    }   
    else
    {   
        for (int i = 0; i < nSamples; ++i)
        {
            cvUnion.value = data[i];
            std::reverse_copy(cvUnion.cArray, cvUnion.cArray + dataTypeSize,
                              result.data() + dataTypeSize*i);
        }
    }   
    return result;
}

template<typename T>
std::string pack(const std::vector<T> &data)
{
    const bool swapBytes
    {   
        std::endian::native == std::endian::little ? false : true
    };  
    return ::pack(data.data(), static_cast<int> (data.size()), swapBytes);
}

}
#endif
