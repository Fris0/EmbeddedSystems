import cv2
import pytesseract
from PIL import Image

cap = cv2.VideoCapture(0)

if not cap.isOpened():
    print("Cannot open camera")
    exit()

# cap.set(cv2.CAP_PROP_FPS, 30)
tesseract_config = r'--psm 4'

while True:
    ret, frame = cap.read()
    if not ret:
        print("Can't receive frame (stream end?). Exiting ...")
        break

    # Process the frame

    frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    frame_im = Image.fromarray(frame)

    text = pytesseract.image_to_data(frame_im)
    print(text)

    # Display the frame
    cv2.imshow('frame', frame)

    # Break the loop with keypress 'q'
    if cv2.waitKey(1) == ord('q'):
        break

# Release the capture
cap.release()
cv2.destroyAllWindows()
