# estimator_pipeline_simulator.py
import numpy as np
from scipy import signal
from collections import deque

WINDOW_SIZE = 5  # number of samples
PRESSURE_RATE_THRESHOLD = 0.1  # kPa/s
FORCE_TO_SENSOR_RATIO = 56.436 # N/kPa, temporary conversion factor based on preliminary data; can be refined with more testing

MPRLS_SAMPLING_RATE_HZ = 100  # Hz
LOWPASS_ORDER = 2
LOWPASS_CUTOFF_FREQ_HZ = 3    # Hz


GRAMS_TO_NEWTONS = 0.00980665  # conversion factor from grams to Newtons
NEWTONS_TO_GRAMS = 1 / GRAMS_TO_NEWTONS

# TODO: Check if this acts exactly the same as iir1 repo
# https://github.com/berndporr/iir1.git
class LowpassFilter:
    def __init__(self, sampling_rate_hz, cutoff_freq_hz, order):
        self.sampling_rate_hz = sampling_rate_hz
        self.cutoff_freq_hz = cutoff_freq_hz
        self.order = order
        self.b, self.a = self.butter_lowpass(sampling_rate_hz, cutoff_freq_hz, order)

    def butter_lowpass(self, fs, cutoff, order):
        """Designs the Butterworth filter coefficients (b, a)."""
        # Nyquist frequency is half the sample rate
        nyq = 0.5 * fs
        # Normalize the cutoff frequency
        normal_cutoff = cutoff / nyq
        # Get the filter coefficients 
        b, a = signal.butter(order, normal_cutoff, btype='low', analog=False)
        return b, a

    def filter(self, data):
        """Applies the forward and backward filter (zero phase) to the signal."""
        # Use filtfilt for zero phase filtering
        y = signal.filtfilt(self.b, self.a, data)
        return y

class EstimatorPipelineSimulator:
    """
    Simulates the estimator pipeline by applying a moving average filter to the input pressure data.
    This is a simple approximation and can be replaced with a more complex model if needed.
    """

    def __init__(self, lowpass_enabled=False, window_size=WINDOW_SIZE):
        self.lowpass_enabled = lowpass_enabled
        self.lp = LowpassFilter(MPRLS_SAMPLING_RATE_HZ, LOWPASS_CUTOFF_FREQ_HZ, LOWPASS_ORDER) if lowpass_enabled else None
        self.window_size = window_size
        self.pressure_kPa_buffer = deque(maxlen=window_size)
        self.rate_kPa_s_buffer = deque(maxlen=window_size)

        self.current_force_N = 0.0
        self.current_kPa = 0.0
        self.prev_kPa = 0.0
        self.zero_kPa = 0.0
        self.ratio = FORCE_TO_SENSOR_RATIO

    def resetZeroLoad(self, pressure_kPa):
        self.current_force_N = 0.0
        self.zero_kPa = pressure_kPa

    def calculateDriftingCompensatedZeroPressure(self, pressure_kPa):
        # TODO: Assume simple linear model where force is proportional to pressure above zero load,
        # and calculate compensated zero load based on current pressure and force
        return pressure_kPa - (self.current_force_N * NEWTONS_TO_GRAMS / self.ratio)

    def process(self, pressure_kPa, pressure_rate_kPa_s):
        # TODO: Assume initial pressure corresponds to zero load,
        # and calculate force based on change in pressure from that point;
        if not self.pressure_kPa_buffer:
            self.resetZeroLoad(pressure_kPa)

        # Update buffers with new data
        self.pressure_kPa_buffer.append(pressure_kPa)
        self.rate_kPa_s_buffer.append(pressure_rate_kPa_s)

        # Return lowpass filtered pressure if enabled, otherwise return raw logged pressure
        if self.lowpass_enabled:
            estimated_pressure_kPa = self.lp.filter(np.array([pressure_kPa]))[0]
        else:
            estimated_pressure_kPa = pressure_kPa

        # Update current force based on pressure and pressure rate
        if abs(np.mean(self.rate_kPa_s_buffer)) > PRESSURE_RATE_THRESHOLD:
            self.current_force_N = (estimated_pressure_kPa - self.zero_kPa) * FORCE_TO_SENSOR_RATIO * GRAMS_TO_NEWTONS
        else:
            self.zero_kPa = self.calculateDriftingCompensatedZeroPressure(estimated_pressure_kPa)

        self.prev_kPa = estimated_pressure_kPa
        return self.current_force_N
