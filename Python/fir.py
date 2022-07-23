def filtre_fir(fe: float):

    # Determine settings for filters
    fir_N = [256, 255, 255, 255, 255]
    fir_Fc = [500, 1000, 2000, 3500, 4490]
    fir_band = [0, 500, 500, 1000, 0]
    fir_type = ["lowpass", "bandpass", "bandpass", "bandpass", "highpass"]
    fir_window = ["blackman", "blackman", "blackman", "blackman", "blackman"]
    # test_sine_freq = [[250, 750, 1000], [250, 1000, 1750], [1000, 2000, 3000], [2000, 3500, 5000], [3000, 4000, 5000]]
    # test_sine = [[]*1024]*3

    # Declare arrays
    fir_h = [[0] * 256] * 5
    fir_h_Q_2_13 = [[0] * 256] * 5
    fir_fft = [[0] * 1024] * 5
    fir_fft_cut = [[0] * 512] * 5

    # Open export file
    fir_export = open("fir_export.txt", "w")

    for i in range(0, len(fir_h)):
        # Get FIR impulse response
        if fir_type[i] == "lowpass" or fir_type[i] == "highpass":
            fir_h[i] = signal.firwin(numtaps=fir_N[i], cutoff=fir_Fc[i], pass_zero=fir_type[i], window=fir_window[i],
                                     fs=fe)
        else:
            fir_h[i] = signal.firwin(numtaps=fir_N[i], cutoff=[fir_Fc[i] - fir_band[i], fir_Fc[i] + fir_band[i]],
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
        plt.ylabel("Gain (db)")

        # Write to export file
        fir_export.write(f"int32_t fir_{i}[{fir_N[i]}]")
        fir_export.write(" = {")

        for j in range(0, fir_N[i]):
            # Reformat to Q2.13
            fir_h_Q_2_13[i][j] = int(np.round(fir_h[i][j] * 2 ** 13))

            # Write to export file
            if j == fir_N[i] - 1:
                fir_export.write(f"{fir_h_Q_2_13[i][j]}")
            else:
                fir_export.write(f"{fir_h_Q_2_13[i][j]}, ")
        fir_export.write("}\n\r")

    # Close export file
    fir_export.close()