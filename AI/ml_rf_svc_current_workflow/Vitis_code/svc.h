#ifndef SVC_PREDICT_H
#define SVC_PREDICT_H

#define MAX_FEATURES 256
#define MAX_CLASSES 32

void svc_predict(
    float x_in[MAX_FEATURES],
    float coef[MAX_CLASSES * MAX_FEATURES],
    float intercept[MAX_CLASSES],
    float mean[MAX_FEATURES],
    float std[MAX_FEATURES],
    int n_features,
    int n_classes,
    int *class_out,
    float scores_out[MAX_CLASSES],
    float *max_confidence,
    float *second_confidence
);

#endif // SVC_PREDICT_H