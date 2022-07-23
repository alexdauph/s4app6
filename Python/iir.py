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
    plt.ylabel("Gain [dB]")
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