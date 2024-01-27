import numpy as np
import cv2
import pytesseract
import datetime
from scipy.ndimage import gaussian_filter
import itertools
import pexpect
import re
import pandas as pd
import os

def run_measurement():
    cap = cv2.VideoCapture(0)

    tesseract_config = r'--psm 3'
    start_time = datetime.datetime.now()

    measurements = []
    while ((datetime.datetime.now() - start_time).total_seconds() < 6):
        # Read in the frame.
        ret, frame = cap.read()
        cv2.startWindowThread()
        # Frame pre-processing.
        gray_image = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        _, binary_image = cv2.threshold(gray_image, 128, 255, cv2.THRESH_BINARY_INV)
        contours, _ = cv2.findContours(binary_image, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        largest_contour = max(contours, key=cv2.contourArea)
        x, y, w, h = cv2.boundingRect(largest_contour)
        roi = frame[y:y+h, x:x+w]

        # Calculate the skew matrix.
        skew_matrix = np.float32([[1, -0.1, 0], [0, 1, 0]])  # -0.4 is the skew-factor.

        # Apply the skew transformation.
        skewed_image = cv2.warpAffine(roi, skew_matrix, (roi.shape[1], roi.shape[0]), borderMode=cv2.BORDER_CONSTANT, borderValue=(255, 255, 255))

        # Crop the image to the appropiate size.
        skewed_image = gaussian_filter(skewed_image, sigma = 1)[509:552, 828:915]


        text = pytesseract.image_to_string(skewed_image)
        try:
            measurements.append(float(text))
        except:
            pass

        # Display the frame
        cv2.imshow('frame', skewed_image)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    # Release the capture
    cap.release()
    cv2.destroyAllWindows()
    return(np.mean(measurements))


def generate_partitions():
    total_layers = 8
    for i in range(1, total_layers):
        for j in range(i, total_layers):
            yield (i, j)


def generate_order():
    hardware = ['B', 'L', 'G']

    order_combinations = list(itertools.permutations(hardware))

    return ['-'.join(order) for order in order_combinations]


def match_output(output, parameter):
    pattern = rf"{re.escape(parameter)}:\s*(\d+\.\d+)"
    match = re.search(pattern, output)
    number = None

    if match:
        number = match.group(1)
        print(parameter, ':', number)
    else:
        print(parameter, 'not found')

    return number


def write_to_excel(parameters, values):
    data = {parameters[i]: [values[i]] for i in range(len(parameters))}

    df = pd.DataFrame(data)

    excel_file = 'measurements.xlsx'

    if os.path.exists(excel_file):
        with pd.ExcelWriter(excel_file, engine='openpyxl', mode='a', if_sheet_exists='overlay') as writer:
            # Write DataFrame to an existing Excel file
            df.to_excel(writer, startrow=writer.sheets['Sheet1'].max_row, index=False, 
                        sheet_name='Sheet1', header= False)
    else:
        df.to_excel(excel_file, index=False)


def run_command(first_partitioning, second_partitioning, order):
    command = f"adb shell 'export LD_LIBRARY_PATH=/data/local/Working_dir; " \
              f"/data/local/Working_dir/graph_alexnet_all_pipe_sync --threads=4 " \
              f"--threads2=2 --n=100 --total_cores=6 --partition_point={first_partitioning} " \
              f"--partition_point2={second_partitioning} --order=={order}'"

    print(command)

    parameters = ['stage1_input_time', 'stage1_inference_time', 'stage1_total_time',
                  'stage2_input_time', 'stage2_inference_time', 'stage2_total_time',
                  'stage3_input_time', 'stage3_inference_time', 'stage3_total_time',
                  'Frame rate is', 'Frame latency is']

    subproc = pexpect.spawn(command, timeout=None, encoding='utf-8')
    values = [first_partitioning, second_partitioning, order]

    while True:
        try:
            index = subproc.expect([pexpect.EOF, pexpect.TIMEOUT, "Running Inference"], timeout=None)
        except KeyboardInterrupt:
            sys.exit(0)

        if index == 0:  # EOF
            output = subproc.before
            print(output)

            for param in parameters:
                values.append(match_output(output, param))

            write_to_excel(['partition point 1', 'partition point 2', 'order', 'power'] 
                           + parameters, values)

            break
        elif index == 2:  # "Running Inference"
            print("Running Inference")
            # Watt used.
            values.append(run_measurement())

    subproc.close()


if __name__ == "__main__":
    # All commands that need to be run in the shell here.

    order_combinations = generate_order()

    # Execute commands for all combinations of partitioning points 
    # and hardware orders.
    for first_point, second_point in generate_partitions():
        for order in order_combinations:
            run_command(first_point, second_point, order)
