import numpy as np
import cv2
import pytesseract
import datetime
import time
import subprocess
from PIL import Image
from scipy.ndimage import gaussian_filter


def run_measurement():
    cap = cv2.VideoCapture(0)

    tesseract_config = r'--psm 3'
    start_time = datetime.datetime.now()

    while ((datetime.datetime.now() - start_time).total_seconds() < 4):
        # Read in the frame.
        ret, frame = cap.read()

        # Frame pre-processing.
        gray_image = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        _, binary_image = cv2.threshold(gray_image, 128, 255, cv2.THRESH_BINARY_INV)
        contours, _ = cv2.findContours(binary_image, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        largest_contour = max(contours, key=cv2.contourArea)
        x, y, w, h = cv2.boundingRect(largest_contour)
        roi = frame[y:y+h, x:x+w]

        # Calculate the skew matrix.
        skew_matrix = np.float32([[1, -0.4, 0], [0, 1, 0]])  # -0.4 is the skew-factor.

        # Apply the skew transformation.
        skewed_image = cv2.warpAffine(roi, skew_matrix, (roi.shape[1], roi.shape[0]), borderMode=cv2.BORDER_CONSTANT, borderValue=(255, 255, 255))

        # Crop the image to the appropiate size.
        skewed_image = gaussian_filter(skewed_image[680:740, 700:825], sigma = 2)

        text = pytesseract.image_to_string(skewed_image)
        try:
            result = float(text)
            print(result)
        except:
            pass

        # Display the frame
        cv2.imshow('frame', skewed_image)

    # Release the capture
    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    run_measurement()
