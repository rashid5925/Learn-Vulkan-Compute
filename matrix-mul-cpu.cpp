#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <stdexcept>


std::vector<float> readCSV(const std::string& filename, size_t& outN) {
    std::vector<float> flatMatrix;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << " for reading.\n";
        outN = 0;
        return flatMatrix;
    }

    std::string line;
    size_t rowCount = 0;
    size_t colCount = 0;

    while (std::getline(file, line)) {
        if (line.empty()) continue; 

        std::stringstream ss(line);
        std::string value;
        size_t currentCw = 0;

        while (std::getline(ss, value, ',')) {
            try {
                flatMatrix.push_back(std::stof(value)); 
                currentCw++;
            } catch (const std::exception& e) {
                std::cerr << "Warning: Skipping invalid token '" << value << "'\n";
            }
        }

        if (currentCw > 0) {
            if (rowCount == 0) {
                colCount = currentCw; 
            }
            rowCount++;
        }
    }

    file.close();

    if (rowCount != colCount) {
        std::cerr << "Warning: Matrix is not square! Rows: " << rowCount << ", Cols: " << colCount << "\n";
    }

    outN = rowCount;
    return flatMatrix;
}

bool writeCSV(const std::string& filename, float* mappedMemory, size_t rows, size_t cols) {
    std::ofstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << " for writing.\n";
        return false;
    }

    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            file << mappedMemory[i * cols + j];
            if (j < cols - 1) {
                file << ",";
            }
        }
        file << "\n";
    }

    file.close();
    return true;
}


std::vector<float> multiplyMatricesCPU(const std::vector<float>& A, const std::vector<float>& B, size_t N) {
    // Initialize output matrix with zeros
    std::vector<float> C(N * N, 0.0f);

    // IKJ Loop Order: Reordered to ensure sequential memory access for cache lines
    for (size_t i = 0; i < N; ++i) {
        for (size_t k = 0; k < N; ++k) {
            float rA = A[i * N + k]; // Cached value from Matrix A
            for (size_t j = 0; j < N; ++j) {
                C[i * N + j] += rA * B[k * N + j];
            }
        }
    }

    return C;
}

int main() {
    std::string inputFilename = "../datasets/input_matrix.csv";
    std::string outputFilename = "../datasets/output_matrix.csv";
    std::cout << "Reading matrix from " << inputFilename << "...\n";
    size_t N;
    std::vector<float> flatInput = readCSV(inputFilename, N);
    if (flatInput.empty()) {
        std::cerr << "Matrix is empty or file error occurred. Exiting.\n";
        return 1;
    }
    std::cout << "Successfully read a matrix of size: " << N << "x" << N << "\n";

    const int bufferSize = ((N * N) + (N * N) + (N * N)) * sizeof(float);

    std::vector<float> flatOutput(N * N, 0.0f);    

    std::vector<float> cpuResult = multiplyMatricesCPU(flatInput, flatInput, N);

    writeCSV("../datasets/cpu_output_matrix.csv", cpuResult.data(), N, N);

    return 0;
}
