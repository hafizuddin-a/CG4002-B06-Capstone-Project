#include <hls_stream.h>
#include "svc.h"

void svc_predict(
    // Input data
    float x_in[MAX_FEATURES],
    
    // Model parameters (via DDR)
    float coef[MAX_CLASSES * MAX_FEATURES],  // Flattened 2D array
    float intercept[MAX_CLASSES],
    float mean[MAX_FEATURES],
    float std[MAX_FEATURES],
    
    // Configuration
    int n_features,
    int n_classes,
    
    // Outputs
    int *class_out,
    float scores_out[MAX_CLASSES],  // All confidence scores
    float *max_confidence,           // Highest score
    float *second_confidence         // Second highest (for margin)
) {
    #pragma HLS INTERFACE m_axi depth=MAX_FEATURES port=x_in offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi depth=MAX_CLASSES*MAX_FEATURES port=coef offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi depth=MAX_CLASSES port=intercept offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi depth=MAX_FEATURES port=mean offset=slave bundle=gmem2
    #pragma HLS INTERFACE m_axi depth=MAX_FEATURES port=std offset=slave bundle=gmem2
    #pragma HLS INTERFACE m_axi depth=MAX_CLASSES port=scores_out offset=slave bundle=gmem3
    
    #pragma HLS INTERFACE s_axilite port=n_features
    #pragma HLS INTERFACE s_axilite port=n_classes
    #pragma HLS INTERFACE s_axilite port=class_out
    #pragma HLS INTERFACE s_axilite port=max_confidence
    #pragma HLS INTERFACE s_axilite port=second_confidence
    #pragma HLS INTERFACE s_axilite port=return
    
    float x_norm[MAX_FEATURES];
    float scores[MAX_CLASSES];
    
    // Normalize
    NORMALIZE: for(int i = 0; i < n_features; i++) {
        #pragma HLS PIPELINE II=1
        x_norm[i] = (x_in[i] - mean[i]) / std[i];
    }
    
    // Compute scores
    COMPUTE_SCORES: for(int c = 0; c < n_classes; c++) {
        float sum = intercept[c];
        DOT_PRODUCT: for(int f = 0; f < n_features; f++) {
            #pragma HLS PIPELINE II=1
            // Access flattened 2D array: coef[c][f] = coef[c * n_features + f]
            sum += coef[c * n_features + f] * x_norm[f];
        }
        scores[c] = sum;
        scores_out[c] = sum;  // Write to output
    }
    
    // Find top 2 for confidence analysis
    int max_idx = 0;
    int second_idx = 1;
    float max_val = scores[0];
    float second_val = scores[1];
    
    if(second_val > max_val) {
        max_idx = 1;
        second_idx = 0;
        max_val = scores[1];
        second_val = scores[0];
    }
    
    ARGMAX: for(int c = 2; c < n_classes; c++) {
        #pragma HLS PIPELINE
        if(scores[c] > max_val) {
            second_val = max_val;
            second_idx = max_idx;
            max_val = scores[c];
            max_idx = c;
        } else if(scores[c] > second_val) {
            second_val = scores[c];
            second_idx = c;
        }
    }
    
    *class_out = max_idx;
    *max_confidence = max_val;
    *second_confidence = second_val;
}