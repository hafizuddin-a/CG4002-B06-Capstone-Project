#include <iostream>
#include <fstream>
#include <cmath>
#include <sstream>
#include <string>
#include "svc.h"

using namespace std;

int main() {
    int n_features = 12;
    int n_classes = 10;
    int num_test_samples = 40;
    
    // Allocate arrays
    float *test_X = new float[num_test_samples * n_features];
    int *test_y = new int[num_test_samples];
    
    float *coef = new float[n_classes * n_features];
    float *intercept = new float[n_classes];
    float *mean = new float[n_features];
    float *std = new float[n_features];
    int *expected_pred = new int[num_test_samples];
    
    // Load model parameters
    ifstream coef_file("test_data_svc/coef.dat");
    ifstream intercept_file("test_data_svc/intercept.dat");
    ifstream mean_file("test_data_svc/mean.dat");
    ifstream std_file("test_data_svc/std.dat");
    
    if(!coef_file.is_open() || !intercept_file.is_open() || 
       !mean_file.is_open() || !std_file.is_open()) {
        cout << "Error: Could not open model parameter files!" << endl;
        return 1;
    }
    
    for(int i = 0; i < n_classes * n_features; i++) {
        coef_file >> coef[i];
    }
    for(int i = 0; i < n_classes; i++) {
        intercept_file >> intercept[i];
    }
    for(int i = 0; i < n_features; i++) {
        mean_file >> mean[i];
        std_file >> std[i];
    }
    
    coef_file.close();
    intercept_file.close();
    mean_file.close();
    std_file.close();
    
    // Load test samples
    ifstream X_file("test_data_svc/X_test.dat");
    ifstream y_file("test_data_svc/y_test.dat");
    ifstream expected_file("test_data_svc/y_pred_expected.dat");
    
    if(!X_file.is_open() || !y_file.is_open() || !expected_file.is_open()) {
        cout << "Error: Could not open test data files!" << endl;
        return 1;
    }
    
    // Read line by line for X (each line is one sample)
    string line;
    int sample_idx = 0;
    while(getline(X_file, line) && sample_idx < num_test_samples) {
        istringstream iss(line);
        for(int f = 0; f < n_features; f++) {
            iss >> test_X[sample_idx * n_features + f];
        }
        sample_idx++;
    }
    
    // Read y values
    for(int i = 0; i < num_test_samples; i++) {
        y_file >> test_y[i];
    }
    
    // Read expected predictions
    for(int i = 0; i < num_test_samples; i++) {
        expected_file >> expected_pred[i];
    }
    
    X_file.close();
    y_file.close();
    expected_file.close();
    
    cout << "\n========================================\n";
    cout << "Data Loading Check\n";
    cout << "========================================\n";
    cout << "First 5 expected predictions: ";
    for(int i = 0; i < 5; i++) {
        cout << expected_pred[i] << " ";
    }
    cout << "\n";
    cout << "First 5 true labels: ";
    for(int i = 0; i < 5; i++) {
        cout << test_y[i] << " ";
    }
    cout << "\n\n";
    
    cout << "First sample features (first 5): ";
    for(int i = 0; i < 5; i++) {
        cout << test_X[i] << " ";
    }
    cout << "\n\n";

    cout << "\n========================================\n";
    cout << "Detailed Debug for Sample 1\n";
    cout << "========================================\n";

    int s = 1;
    float x_in[MAX_FEATURES];
    float x_norm[MAX_FEATURES];

    // Load sample
    for(int i = 0; i < n_features; i++) {
        x_in[i] = test_X[s * n_features + i];
    }

    // Manual normalization
    cout << "Input (first 5): ";
    for(int i = 0; i < 5; i++) {
        cout << x_in[i] << " ";
    }
    cout << "\n";

    cout << "Mean (first 5): ";
    for(int i = 0; i < 5; i++) {
        cout << mean[i] << " ";
    }
    cout << "\n";

    cout << "Std (first 5): ";
    for(int i = 0; i < 5; i++) {
        cout << std[i] << " ";
    }
    cout << "\n";

    cout << "Normalized (first 5): ";
    for(int i = 0; i < n_features; i++) {
        x_norm[i] = (x_in[i] - mean[i]) / std[i];
        if(i < 5) cout << x_norm[i] << " ";
    }
    cout << "\n\n";

    // Manual score computation
    cout << "Manual score computation:\n";
    for(int c = 0; c < n_classes; c++) {
        float sum = intercept[c];
        cout << "Class " << c << ": intercept=" << intercept[c];
        
        for(int f = 0; f < n_features; f++) {
            float weight = coef[c * n_features + f];
            sum += weight * x_norm[f];
            if(f < 3) {
                cout << ", w[" << f << "]=" << weight 
                    << " * x[" << f << "]=" << x_norm[f];
            }
        }
        cout << " -> score=" << sum << "\n";
    }

    // Now run HLS
    float scores_out[MAX_CLASSES];
    int predicted_class;
    float max_conf, second_conf;

    svc_predict(x_in, coef, intercept, mean, std,
            n_features, n_classes,
            &predicted_class, scores_out, 
            &max_conf, &second_conf);

    cout << "\nHLS scores: ";
    for(int c = 0; c < n_classes; c++) {
        cout << scores_out[c] << " ";
    }
    cout << "\nHLS predicted: " << predicted_class << "\n";
    
    // Run inference
    cout << "========================================\n";
    cout << "First 5 samples - detailed comparison:\n";
    cout << "========================================\n";
    
    int correct = 0;
    int python_match = 0;
    
    for(int s = 0; s < min(5, num_test_samples); s++) {
        float x_in[MAX_FEATURES];
        float scores_out[MAX_CLASSES];
        int predicted_class;
        float max_conf, second_conf;
        
        for(int i = 0; i < n_features; i++) {
            x_in[i] = test_X[s * n_features + i];
        }
        
        svc_predict(x_in, coef, intercept, mean, std,
                   n_features, n_classes,
                   &predicted_class, scores_out, 
                   &max_conf, &second_conf);
        
        cout << "\nSample " << s << ":\n";
        cout << "  True class: " << test_y[s] << "\n";
        cout << "  Python predicted: " << expected_pred[s] << "\n";
        cout << "  HLS predicted: " << predicted_class << "\n";
        cout << "  Scores: ";
        for(int c = 0; c < n_classes; c++) {
            cout << scores_out[c] << " ";
        }
        cout << "\n";
        
        if(predicted_class == test_y[s]) {
            cout << "  ✓ Correct vs ground truth\n";
            correct++;
        } else {
            cout << "  ✗ Wrong vs ground truth\n";
        }
        
        if(predicted_class == expected_pred[s]) {
            cout << "  ✓ Match with Python\n";
            python_match++;
        } else {
            cout << "  ✗ Mismatch with Python\n";
        }
    }
    
    // Run full test
    cout << "\n========================================\n";
    cout << "Running Full Test\n";
    cout << "========================================\n";
    
    correct = 0;
    python_match = 0;
    
    for(int s = 0; s < num_test_samples; s++) {
        float x_in[MAX_FEATURES];
        float scores_out[MAX_CLASSES];
        int predicted_class;
        float max_conf, second_conf;
        
        for(int i = 0; i < n_features; i++) {
            x_in[i] = test_X[s * n_features + i];
        }
        
        svc_predict(x_in, coef, intercept, mean, std,
                   n_features, n_classes,
                   &predicted_class, scores_out, 
                   &max_conf, &second_conf);
        
        if(predicted_class == test_y[s]) {
            correct++;
        }
        
        if(predicted_class == expected_pred[s]) {
            python_match++;
        }
    }
    
    float accuracy = (float)correct / num_test_samples;
    float python_agreement = (float)python_match / num_test_samples;
    
    cout << "\n========================================\n";
    cout << "Results\n";
    cout << "========================================\n";
    cout << "HLS vs Ground Truth: " << correct << "/" << num_test_samples 
         << " (" << (accuracy * 100.0) << "%)\n";
    cout << "HLS vs Python: " << python_match << "/" << num_test_samples 
         << " (" << (python_agreement * 100.0) << "%)\n";
    
    delete[] test_X;
    delete[] test_y;
    delete[] expected_pred;
    delete[] coef;
    delete[] intercept;
    delete[] mean;
    delete[] std;
    
    if(python_agreement > 0.95) {  // 95% agreement with Python
        cout << "\n✓ PASS: HLS matches Python implementation\n";
        return 0;
    } else {
        cout << "\n✗ FAIL: HLS differs from Python\n";
        return 1;
    }
}