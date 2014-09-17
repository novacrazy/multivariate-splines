/*
This file is part of the Multivariate Splines library.
Copyright (C) 2012 Bjarne Grimstad (bjarne.grimstad@gmail.com)

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "datatable.h"

#include <string>
#include <fstream>
#include <stdexcept>
#include <limits>

namespace MultivariateSplines
{

/*
    To ensure greater platform support, conversion between endianness and data type sizes is supported.
*/

struct BinaryArch_Header {
    //Not exactly necessary, but might be useful at some point.
    uint8_t word_size, double_size;

    BinaryArch_Header()
        : word_size(sizeof(unsigned int)), double_size(sizeof(double))
    {
    }
};

static const uint16_t bigEndianBOM     = 0xFEFF;
static const uint16_t littleEndianBOM  = 0xFFFE;

inline bool isBigEndianBOM(const uint16_t& bom)
{
    return bom == bigEndianBOM;
}

inline bool isLittleEndianBOM(const uint16_t& bom)
{
    return bom == littleEndianBOM;
}

/*
    Taken from:

    outFile << "# Saved DataTable" << '\n';
    outFile << "# Number of samples: " << getNumSamples() << '\n';
    outFile << "# Complete grid: " << (isGridComplete() ? "yes" : "no") << '\n';
    outFile << "# xDim: " << numVariables << '\n';
    outFile << numVariables << " " << 1 << '\n';
*/

struct DataTable_Header {
    //Actually a BOM, but suitable for ensuring the header was loaded fully
    uint16_t magic_const;

    BinaryArch_Header arch;

    //size_t would normally work, but this HAS to be cross-arch
    uint64_t samples, xDim, yDim;
    bool complete;

    DataTable_Header(uint64_t s, uint64_t x, uint64_t y, bool c)
        : samples(s), xDim(x), yDim(y), complete(c)
    {
        this->magic_const = Arch::isLittleEndian() ? littleEndianBOM : bigEndianBOM;
    }

    DataTable_Header()
        : DataTable_Header(0, 0, 0, 0)
    {
    }
};

DataTable::DataTable()
    : DataTable(false, false)
{
}

DataTable::DataTable(bool allowDuplicates)
    : DataTable(allowDuplicates, false)
{
}

DataTable::DataTable(bool allowDuplicates, bool allowIncompleteGrid)
    : allowDuplicates(allowDuplicates),
      allowIncompleteGrid(allowIncompleteGrid),
      numDuplicates(0),
      numVariables(0)
{
}

void DataTable::addSample(double x, double y)
{
    addSample(DataSample(x, y));
}

void DataTable::addSample(std::vector<double> x, double y)
{
    addSample(DataSample(x, y));
}

void DataTable::addSample(DenseVector x, double y)
{
    addSample(DataSample(x, y));
}

void DataTable::addSample(const DataSample &sample)
{
    if(getNumSamples() == 0)
    {
        numVariables = sample.getDimX();
        initDataStructures();
    }

    assert(sample.getDimX() == numVariables); // All points must have the same dimension

    // Check if the sample has been added already
    if(samples.count(sample) > 0)
    {
        if(!allowDuplicates)
        {
            std::cout << "Discarding duplicate sample because allowDuplicates is false!" << std::endl;
            std::cout << "Initialise with DataTable(true) to set it to true." << std::endl;
            return;
        }

        numDuplicates++;
    }

    samples.insert(sample);

    recordGridPoint(sample);
}

void DataTable::recordGridPoint(const DataSample &sample)
{
    for(unsigned int i = 0; i < getNumVariables(); i++)
    {
        grid.at(i).insert(sample.getX().at(i));
    }
}

unsigned int DataTable::getNumSamplesRequired() const
{
    unsigned long samplesRequired = 1;
    unsigned int i = 0;
    for(auto &variable : grid)
    {
        samplesRequired *= (unsigned long) variable.size();
        i++;
    }

    return (i > 0 ? samplesRequired : (unsigned long) 0);
}

bool DataTable::isGridComplete() const
{
    return samples.size() > 0 && samples.size() - numDuplicates == getNumSamplesRequired();
}

void DataTable::initDataStructures()
{
    for(unsigned int i = 0; i < getNumVariables(); i++)
    {
        grid.push_back(std::set<double>());
    }
}

void DataTable::gridCompleteGuard() const
{
    if(!isGridComplete() && !allowIncompleteGrid)
    {
        std::cout << "The grid is not complete yet!" << std::endl;
        exit(1);
    }
}

/***********
 * Getters *
 ***********/

std::multiset<DataSample>::const_iterator DataTable::cbegin() const
{
    gridCompleteGuard();

    return samples.cbegin();
}

std::multiset<DataSample>::const_iterator DataTable::cend() const
{
    gridCompleteGuard();

    return samples.cend();
}

// Get table of samples x-values,
// i.e. table[i][j] is the value of variable i at sample j
std::vector< std::vector<double> > DataTable::getTableX() const
{
    gridCompleteGuard();

    // Initialize table
    std::vector<std::vector<double>> table;
    for(unsigned int i = 0; i < numVariables; i++)
    {
        std::vector<double> xi(getNumSamples(), 0.0);
        table.push_back(xi);
    }

    // Fill table with values
    int i = 0;
    for(auto &sample : samples)
    {
        std::vector<double> x = sample.getX();

        for(unsigned int j = 0; j < numVariables; j++)
        {
            table.at(j).at(i) = x.at(j);
        }
        i++;
    }

    return table;
}

// Get vector of y-values
std::vector<double> DataTable::getVectorY() const
{
    std::vector<double> y;
    for(std::multiset<DataSample>::const_iterator it = cbegin(); it != cend(); ++it)
    {
        y.push_back(it->getY());
    }
    return y;
}

/*****************
 * Save and load *
 *****************/

void DataTable::save(std::string fileName, std::ios_base::openmode mode) const
{

    // To give a consistent format across all locales, use the C locale when saving and loading
    std::locale current_locale = std::locale::global(std::locale("C"));

    std::ofstream outFile;

    try
    {
        outFile.open(fileName, mode | std::ios_base::out | std::ios_base::trunc);
    }
    catch(const std::ios_base::failure &e)
    {
        throw;
    }

    if(mode & std::ios_base::binary)
    {
        DataTable_Header hdr(getNumSamples(), numVariables, 1, isGridComplete());

        outFile.write(reinterpret_cast<const char*>(&hdr), sizeof(DataTable_Header));

        double tmp;

        for(auto it = cbegin(); it != cend(); it++)
        {
            //Since this is a binary write, it can write directly from the data buffer in std::vector
            outFile.write(reinterpret_cast<const char*>(it->getX().data()), sizeof(std::vector<double>::value_type) * hdr.xDim);

            tmp = it->getY();

            outFile.write(reinterpret_cast<const char*>(&tmp), sizeof(double));
        }
    }
    else
    {

        // If this function is still alive the file must be open,
        // no need to call is_open()

        // Write header
        outFile << "# Saved DataTable" << '\n';
        outFile << "# Number of samples: " << getNumSamples() << '\n';
        outFile << "# Complete grid: " << (isGridComplete() ? "yes" : "no") << '\n';
        outFile << "# xDim: " << numVariables << '\n';
        outFile << numVariables << " " << 1 << '\n';

        for(auto it = cbegin(); it != cend(); it++)
        {
            for(unsigned int i = 0; i < numVariables; i++)
            {
                outFile << std::setprecision(SAVE_DOUBLE_PRECISION) << it->getX().at(i) << " ";
            }

            outFile << std::setprecision(SAVE_DOUBLE_PRECISION) << it->getY();

            outFile << '\n';
        }
    }

    // close() also flushes
    try
    {
        outFile.close();
    }
    catch(const std::ios_base::failure &e)
    {
        throw;
    }

    std::locale::global(current_locale);
}

void DataTable::load(std::string fileName, std::ios_base::openmode mode)
{

    // To give a consistent format across all locales, use the C locale when saving and loading
    std::locale current_locale = std::locale::global(std::locale("C"));

    std::ifstream inFile;

    try
    {
        inFile.open(fileName, mode | std::ios_base::in);
    }
    catch(const std::ios_base::failure &e)
    {
        throw;
    }

    if(mode & std::ios_base::binary)
    {
        DataTable_Header hdr;

        inFile.seekg(0);

        inFile.read(reinterpret_cast<char*>(&hdr), sizeof(DataTable_Header));

        std::cout << "0x" << std::hex << hdr.magic_const << std::endl;

        std::cout << inFile.bad() << std::endl << inFile.fail() << std::endl << inFile.rdstate() << std::endl;

        if(inFile.good() && !inFile.eof())
        {
            if(isBigEndianBOM(hdr.magic_const) || isLittleEndianBOM(hdr.magic_const))
            {

                bool needs_endian_conversion = Arch::isLittleEndian() != isLittleEndianBOM(hdr.magic_const);
                bool needs_size_conversion = sizeof(double) != hdr.arch.double_size;


                std::cout << "needs_endian_conversion: " << std::boolalpha << needs_endian_conversion << std::endl;
                std::cout << "needs_size_conversion: " << std::boolalpha << needs_size_conversion << std::endl;

                std::cout << "xDim: " << std::dec << hdr.xDim << std::endl;
                std::cout << "yDim: " << hdr.yDim << std::endl;
                std::cout << "samples: " << hdr.samples << std::endl;
                std::cout << "complete: " << std::boolalpha << hdr.complete << std::endl;

                size_t count = 0;

                size_t len = hdr.xDim + hdr.yDim;

                //allocate enough for both dimensions to be loaded at once
                //and since the dimensions don't change, we can re-use this buffer
                double* tmp = new double[len];

                memset(tmp, 0x0, sizeof(double) * len);

                std::cout << "Starting read..." << std::endl;

                inFile.read(reinterpret_cast<char*>(tmp), len * sizeof(double));

                while(inFile.good() && !inFile.eof() && (count++ < hdr.samples))
                {
                    if(needs_endian_conversion) {
                        std::for_each(tmp, &tmp[len], Arch::endian_reverse<double>::swap);
                    }

                    auto x = std::vector<double>(tmp, &tmp[hdr.xDim]);

                    addSample(x, tmp[hdr.xDim]);

                    inFile.read(reinterpret_cast<char*>(tmp), len * sizeof(double));
                }

                delete[] tmp;
            }
            else
            {
                throw std::out_of_range("Invalid BOM");
            }
        }
        else
        {
            throw std::runtime_error("DataTable::load: Failed to read binary header");
        }

    }
    else
    {

        // If this function is still alive the file must be open,
        // no need to call is_open()

        // Skip past comments
        std::string line;

        int nX, nY;
        int state = 0;
        while(std::getline(inFile, line))
        {
            // Look for comment sign
            if(line.at(0) == '#')
            {
                continue;
            }

            // Reading number of dimensions
            else if(state == 0)
            {
                nX = checked_strtol(line.c_str());
                nY = 1;
                state = 1;
            }

            // Reading samples
            else if(state == 1)
            {
                auto x = std::vector<double>(nX);
                auto y = std::vector<double>(nY);

                const char* str = line.c_str();
                char* nextStr = nullptr;

                for(int i = 0; i < nX; i++)
                {
                    x.at(i) = checked_strtod(str, &nextStr);
                    str = nextStr;
                }
                for(int j = 0; j < nY; j++)
                {
                    y.at(j) = checked_strtod(str, &nextStr);
                    str = nextStr;
                }

                addSample(x, y.at(0));
            }
        }
    }

    // close() also flushes
    try
    {
        inFile.close();
    }
    catch(const std::ios_base::failure &e)
    {
        throw;
    }

    std::locale::global(current_locale);
}

/**************
 * Debug code *
 **************/
void DataTable::printSamples() const
{
    for(auto &sample : samples)
    {
        std::cout << sample << std::endl;
    }
}

void DataTable::printGrid() const
{
    std::cout << "===== Printing grid =====" << std::endl;

    unsigned int i = 0;
    for(auto &variable : grid)
    {
        std::cout << "x" << i++ << "(" << variable.size() << "): ";
        for(double value : variable)
        {
            std::cout << value << " ";
        }
        std::cout << std::endl;
    }

    std::cout << "Unique samples added: " << samples.size() << std::endl;
    std::cout << "Samples required: " << getNumSamplesRequired() << std::endl;
}

} // namespace MultivariateSplines
