import matplotlib.pyplot as plt
import numpy as np
from scipy import signal
from zplane import zplane


def filtre_fir(fe: float):
    # Declare arrays
    fir_h = [[0] * 256] * 5
    fir_h_q_2_13 = [[0] * 256] * 5
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
    fir_export = open("fir_export.txt", "w")

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
        fir_export.write(f"int32_t fir_{i}[{fir_N[i]}]")
        fir_export.write(" = {")

        for j in range(0, fir_N[i]):
            # Reformat to Q2.13
            fir_h_q_2_13[i][j] = int(np.round(fir_h[i][j] * 2 ** 13))

            # Write to export file
            if j == fir_N[i] - 1:
                fir_export.write(f"{fir_h_q_2_13[i][j]}")
            else:
                fir_export.write(f"{fir_h_q_2_13[i][j]}, ")
        fir_export.write("}\n\r")

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
    iir_b = [0] * 9
    iir_a_q_2_13 = [0] * 9
    iir_b_q_2_13 = [0] * 9

    # Filter specifications
    iir_fc_low: float = 950
    iir_fc_high: float = 1050
    iir_N: int = 4
    iir_pass_ripple: float = 1
    irr_stop_attn: float = 70

    # Get a and b parameters
    [iir_b, iir_a] = signal.ellip(
        N=iir_N,
        rp=iir_pass_ripple,
        rs=irr_stop_attn,
        Wn=[iir_fc_low, iir_fc_high],
        fs=fe,
        btype="bandstop",
        output="ba",
    )

    # Plot frequency response
    [w, h_dft] = signal.freqz(iir_b, iir_a, worN=1000000, fs=fe)
    plt.figure()
    plt.semilogx(w, 20 * np.log10(np.abs(h_dft)))
    plt.title(f"Réponse en fréquence du filtre elliptique (ordre {iir_N})")
    plt.xlabel("Fréquence [Hz]")
    plt.ylabel("Gain (dB)")
    plt.grid(which="both", axis="both")

    # Open export file
    iir_export = open("iir_export.txt", "w")

    # Write a to export file
    iir_export.write(f"int32_t iir_a[{len(iir_a)}]")
    iir_export.write(" = {")
    for i in range(0, len(iir_a)):
        iir_a_q_2_13[i] = int(np.round(iir_a[i] * 2 ** 13))
        if i == len(iir_a) - 1:
            iir_export.write(f"{iir_a_q_2_13[i]}")
        else:
            iir_export.write(f"{iir_a_q_2_13[i]}, ")
    iir_export.write("}\n\r")

    # Write b to export file
    iir_export.write(f"int32_t iir_b[{len(iir_b)}]")
    iir_export.write(" = {")
    for i in range(0, len(iir_b)):
        iir_b_q_2_13[i] = int(np.round(iir_b[i] * 2 ** 13))
        if i == len(iir_b) - 1:
            iir_export.write(f"{iir_b_q_2_13[i]}")
        else:
            iir_export.write(f"{iir_b_q_2_13[i]}, ")
    iir_export.write("}\n\r")

    # Close export file
    iir_export.close()


def s4app6():
    plt.ion()  # Comment out if using scientific mode!

    fe = 20000
    filtre_fir(fe)
    print("Done!")


if __name__ == "__main__":
    s4app6()
