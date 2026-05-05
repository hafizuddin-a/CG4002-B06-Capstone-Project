## Current Workflow for AI (ml_rf_svc_current_workflow)
1. **Collect data from UART/MQTT (need more interfacing)**  
   - Folder: `Data_collection_code`  
   - Note: Use together with `Hardware/FireBeetle-ESP32_CollectData.ino` with Serial.print instead of publishing messages.

2. **Train SVC model in Python**  
   - Folder: `Python_training_code`  
   - Note: Trained with the collected data placed in `data` folder. Save model parameters and class_txt files.

3. **Run HLS C++ code on Vitis to generate HDL for Vivado**
   - Folder: `Vitis_code`  
   - Note: Set the correct (Ultra96v2) part number, run synthesis, testbench on your data, export the IP block to Vivado

4. **Connect IP blocks in Vivado, run synthesis, implementation, and export bitstream**

5. **Deploy and run the bistream on Ultra96v2 with PYNQ**
   - Folder: `Ultra96v2_code`  
   - Note: Initialize the accelerator, give in the bitstream path and the model parameters and class_txt file.


## Old Workflow for AI (hls4ml_cnn1d_old_workflow)

*(Glove Gesture Dataset: [Kaggle link](https://www.kaggle.com/datasets/mouadfiali/sensor-based-american-sign-language-recognition))*

1. **Train a model in Python and convert it to C++ (HLS) code**  
   - Folder: `hls4ml_WithSampleDataInput`  
   - Note: Trained using the gloves gesture dataset.

2. **Run testbench (Python → C++ and C++ → HDL), synthesize, and export HLS code to Verilog/VHDL**  
   - Folder: `Vitis_WithSampleDataInput`  
   - Note: Trained using the gloves gesture dataset.

3. **Connect IP blocks in Vivado, run synthesis, implementation, and export bitstream**

4. **Deploy and run bitstream on Ultra96 with PYNQ**

---
