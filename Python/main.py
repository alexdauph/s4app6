import matplotlib.pyplot as plt
import numpy as np
from scipy import signal
from zplane import zplane


def filtre_fir(fe: float):
    # Declare arrays
    fir_h = [[0] * 256] * 5
    fir_fft = [[0] * 1024] * 5
    fir_fft_cut = [[0] * 512] * 5

    # Filter specifications
    fir_N = [256, 256, 256, 256, 255]
    fir_fc = [500, 1000, 2000, 3500, 4490]
    fir_band = [0, 500, 500, 1000, 0]
    fir_type = ["lowpass", "bandpass", "bandpass", "bandpass", "highpass"]
    fir_window = ["blackman", "blackman", "blackman", "blackman", "blackman"]
    # test_sine_freq = [[250, 750, 1000], [250, 1000, 1750], [1000, 2000, 3000], [2000, 3500, 5000], [3000, 4000, 5000]]
    # test_sine = [[]*1024]*3

    # Open export file
    fir_export = open("filterFIRcoeffs.h", "w")
    fir_export.write("#define H_and_W_QXY_RES_NBITS 13 // Q2.13\n")

    for i in range(0, len(fir_h)):
        # Get FIR impulse response
        if fir_type[i] == "lowpass" or fir_type[i] == "highpass":
            fir_h[i] = signal.firwin(numtaps=fir_N[i], cutoff=fir_fc[i], pass_zero=fir_type[i], window=fir_window[i],
                                     fs=fe)
        else:
            fir_h[i] = signal.firwin(numtaps=fir_N[i], cutoff=[fir_fc[i] - fir_band[i], fir_fc[i] + fir_band[i]],
                                     pass_zero=fir_type[i], window=fir_window[i], fs=fe)

        # Plot impulse response
        plt.figure()
        plt.subplot(2, 1, 1)
        plt.plot(fir_h[i])
        plt.title(f"Filtre FIR[{i}] {fir_type[i]} - Réponse impulsionelle")
        plt.xlabel("n")
        plt.ylabel("Amplitude")

        # Get FIR frequency response
        fir_h[i] = np.append(fir_h[i], np.zeros(1024 - len(fir_h[i])))
        fir_fft[i] = np.fft.fft(fir_h[i])
        fir_fft_cut[i] = fir_fft[i][0:512]

        # Plot frequency response
        plt.subplot(2, 1, 2)
        plt.semilogx(np.arange(0, 512) / 512 * fe / 2, 20 * np.log10(abs(fir_fft_cut[i])))
        plt.title(f"Filtre FIR[{i}] {fir_type[i]} - Réponse en fréquence")
        plt.xlabel("Fréquence (Hz)")
        plt.ylabel("Gain (dB)")

        # Write to export file
        fir_export.write(f"// {fir_type[i]} filter ({fir_window[i]} window), fc = {fir_fc[i]} Hz, band = {fir_band[i]} Hz, fe = {fe} Hz\n")
        fir_export.write(f"const int32c H{7-i}[FFT_LEN]")
        fir_export.write(" = {\n")

        for j in range(0, 1024):
            # Reformat to Q2.13
            re = int(np.round(fir_fft[i][j] * 2 ** 13).real)
            im = int(np.round(fir_fft[i][j] * 2 ** 13).imag)
            fir_export.write("\t{")
            fir_export.write(f"{re}, {im}")
            fir_export.write("},\n")
        fir_export.write("};\n\n")

    # Close export file
    fir_export.close()

    # Plot frequency response
    plt.figure()
    plt.semilogx(np.arange(0, 512) / 512 * fe / 2, 20 * np.log10(abs(fir_fft_cut[0])))
    plt.semilogx(np.arange(0, 512) / 512 * fe / 2, 20 * np.log10(abs(fir_fft_cut[1])))
    plt.semilogx(np.arange(0, 512) / 512 * fe / 2, 20 * np.log10(abs(fir_fft_cut[2])))
    plt.semilogx(np.arange(0, 512) / 512 * fe / 2, 20 * np.log10(abs(fir_fft_cut[3])))
    plt.semilogx(np.arange(0, 512) / 512 * fe / 2, 20 * np.log10(abs(fir_fft_cut[4])))
    plt.title(f"Filtres FIR - Réponse en fréquence")
    plt.xlabel("Fréquence (Hz)")
    plt.ylabel("Gain (dB)")
    plt.grid()
    plt.tight_layout()


def filtre_iir(fe: float):
    # Declare arrays
    iir_a = [0] * 9
    iir_SOS = [[0] * 6] * 4

    # Filter specifications
    iir_fc_low: float = 950
    iir_fc_high: float = 1050
    iir_N: int = 4
    iir_pass_ripple: float = 1
    irr_stop_attn: float = 70

    # Get sos parameters
    iir_SOS = signal.ellip(
        N=iir_N,
        rp=iir_pass_ripple,
        rs=irr_stop_attn,
        Wn=[iir_fc_low, iir_fc_high],
        fs=fe,
        btype="bandstop",
        output="sos",
    )

    # Plot frequency response
    [iir_w, iir_fft] = signal.sosfreqz(iir_SOS, worN=100000, fs=fe)
    plt.figure()
    plt.semilogx(iir_w, 20 * np.log10(np.abs(iir_fft)))
    plt.title(f"Réponse en fréquence du filtre elliptique (ordre {iir_N})")
    plt.xlabel("Fréquence [Hz]")
    plt.ylabel("Gain (dB)")
    plt.grid(which="both", axis="both")

    # Open export file
    iir_export = open("filterIIRcoeffs.h", "w")
    iir_export.write("// IIRCoeffs : coefficients (b0, b1, b2, a0, a1, a2) for N_SOS_SECTIONS cascaded SOS sections\n")
    iir_export.write("#define IIR_QXY_RES_NBITS 13 // Q2.13\n")
    iir_export.write("#define N_SOS_SECTIONS 4\n")

    # Write SOS to export file
    iir_export.write("int32_t IIRCoeffs[N_SOS_SECTIONS][6] = {\n")
    for i in range(0, len(iir_SOS)):
        iir_export.write("\t{")
        for j in range(0, len(iir_SOS[0])):
            val = int(np.round(iir_SOS[i][j] * 2 ** 13))
            if j == len(iir_SOS[0]) - 1:
                iir_export.write(f"{val}")
            else:
                iir_export.write(f"{val}, ")
        iir_export.write("},\n")
    iir_export.write("};\n")
    iir_export.write("int32_t IIRu[N_SOS_SECTIONS] = {0}, IIRv[N_SOS_SECTIONS] = {0};")

    # Close export file
    iir_export.close()


def s4app6():
    plt.ion()  # Comment out if using scientific mode!

    fe = 20000
    filtre_fir(fe)
    print("Done!")


if __name__ == "__main__":
    s4app6()
