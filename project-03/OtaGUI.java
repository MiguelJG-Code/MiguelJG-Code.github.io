/*
 * ESP32 OTA + Telemetry GUI
 *
 * Tab 1 — TELEMETRY : live sensor bars (3× ultrasonic) + IR indicators
 *                     connected via plain TCP (port 81), newline-delimited JSON
 * Tab 2 — OTA       : pick a .bin, ping the ESP32, flash over WiFi
 *
 * Compile & run:
 *   javac OtaGUI.java
 *   java  OtaGUI
 */

import javax.swing.*;
import javax.swing.border.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.net.*;
import java.util.concurrent.atomic.*;

public class OtaGUI extends JFrame {

    // ── Theme ─────────────────────────────────────────────────────────
    static final Color BG       = new Color(0x0D, 0x0D, 0x0D);
    static final Color PANEL_BG = new Color(0x17, 0x17, 0x1A);
    static final Color ACCENT   = new Color(0x00, 0xE5, 0xFF);
    static final Color ACCENT2  = new Color(0xFF, 0x45, 0x00);
    static final Color BTN_BG   = new Color(0x1E, 0x1E, 0x26);
    static final Color BTN_HOV  = new Color(0x2A, 0x2A, 0x38);
    static final Color TEXT     = new Color(0xE8, 0xE8, 0xE8);
    static final Color DIM      = new Color(0x55, 0x55, 0x66);
    static final Color GREEN    = new Color(0x00, 0xFF, 0x88);
    static final Color RED      = new Color(0xFF, 0x30, 0x50);
    static final Color WARN     = new Color(0xFF, 0xAA, 0x00);

    static final Font F_TITLE = new Font("Monospaced", Font.BOLD, 20);
    static final Font F_LABEL = new Font("Monospaced", Font.BOLD, 11);
    static final Font F_SMALL = new Font("Monospaced", Font.PLAIN, 10);

    // ── Shared IP field ───────────────────────────────────────────────
    private JTextField ipField;

    // ── OTA widgets ───────────────────────────────────────────────────
    private File       selectedBin;
    private JLabel     fileLabel, pingStatus, uploadStatus;
    private JProgressBar progress;
    private JButton    pickBtn, uploadBtn, pingBtn;

    // ── Telemetry widgets ─────────────────────────────────────────────
    private SensorBar  usBar1, usBar2, usBar3;
    private IrLight    irLeft, irRight;
    private JLabel     tcpStatus;
    private JLabel     modeLabel;      // shows current bot mode
    private String     currentMode = "NONE";
    private JButton    tcpConnBtn;
    private TcpClient  tcpClient;

    // ─────────────────────────────────────────────────────────────────
    public OtaGUI() {
        super("ESP32 OTA + Telemetry");
        setDefaultCloseOperation(EXIT_ON_CLOSE);
        setSize(560, 520);
        setResizable(false);
        setLocationRelativeTo(null);

        JPanel root = new JPanel(new BorderLayout());
        root.setBackground(BG);
        root.add(buildHeader(),  BorderLayout.NORTH);
        root.add(buildIpRow(),   BorderLayout.CENTER);
        setContentPane(root);
    }

    // ── Header ────────────────────────────────────────────────────────
    private JPanel buildHeader() {
        JPanel p = new JPanel(new BorderLayout());
        p.setBackground(PANEL_BG);
        p.setBorder(BorderFactory.createCompoundBorder(
            new MatteBorder(0, 0, 1, 0, ACCENT),
            new EmptyBorder(12, 20, 12, 20)));
        JLabel t = new JLabel("◈  ESP32 OTA + TELEMETRY  ◈");
        t.setFont(F_TITLE); t.setForeground(ACCENT);
        p.add(t, BorderLayout.WEST);
        return p;
    }

    // ── IP row + tabbed pane ──────────────────────────────────────────
    private JPanel buildIpRow() {
        JPanel wrapper = new JPanel(new BorderLayout());
        wrapper.setBackground(BG);

        // IP bar
        JPanel ipBar = new JPanel(new FlowLayout(FlowLayout.CENTER, 10, 10));
        ipBar.setBackground(PANEL_BG);
        ipBar.setBorder(new MatteBorder(0, 0, 1, 0, new Color(0x28, 0x28, 0x35)));

        JLabel ipLbl = new JLabel("ESP32 IP:");
        ipLbl.setFont(F_LABEL); ipLbl.setForeground(DIM);

        ipField = new JTextField("192.168.137.", 14);
        ipField.setFont(F_LABEL);
        ipField.setBackground(BTN_BG); ipField.setForeground(ACCENT);
        ipField.setCaretColor(ACCENT);
        ipField.setBorder(BorderFactory.createCompoundBorder(
            new LineBorder(new Color(0x28, 0x28, 0x35)),
            new EmptyBorder(4, 6, 4, 6)));

        pingBtn = mkBtn("⚡ PING", ACCENT2);
        pingBtn.setPreferredSize(new Dimension(90, 26));
        pingBtn.addActionListener(e -> doPing());

        pingStatus = new JLabel("Not checked");
        pingStatus.setFont(F_LABEL); pingStatus.setForeground(DIM);

        ipBar.add(ipLbl); ipBar.add(ipField);
        ipBar.add(pingBtn); ipBar.add(pingStatus);
        wrapper.add(ipBar, BorderLayout.NORTH);

        // Tabs — Telemetry first, OTA second
        JTabbedPane tabs = new JTabbedPane();
        tabs.setBackground(BG); tabs.setForeground(TEXT);
        tabs.setFont(F_LABEL);
        UIManager.put("TabbedPane.selected",            PANEL_BG);
        UIManager.put("TabbedPane.background",          BG);
        UIManager.put("TabbedPane.foreground",          TEXT);
        UIManager.put("TabbedPane.contentBorderInsets", new Insets(0, 0, 0, 0));

        tabs.addTab("  TELEMETRY  ", buildTelemetryTab());   // index 0 — default
        tabs.addTab("  OTA  ",       buildOtaTab());          // index 1
        tabs.setSelectedIndex(0);

        wrapper.add(tabs, BorderLayout.CENTER);
        return wrapper;
    }

    // ══════════════════════════════════════════════════════════════════
    //  TELEMETRY TAB  (tab 0 — default)
    // ══════════════════════════════════════════════════════════════════
    private JPanel buildTelemetryTab() {
        JPanel p = new JPanel(new BorderLayout(0, 10));
        p.setBackground(BG);
        p.setBorder(new EmptyBorder(14, 14, 14, 14));

        // ── Top: mode badge + connect button + status ─────────────────
        JPanel topWrapper = new JPanel();
        topWrapper.setLayout(new BoxLayout(topWrapper, BoxLayout.Y_AXIS));
        topWrapper.setOpaque(false);

        // Mode badge row
        JPanel modeRow = new JPanel(new FlowLayout(FlowLayout.CENTER, 0, 4));
        modeRow.setOpaque(false);
        modeLabel = new JLabel(" ◈  MODE:  NONE  ◈ ");
        modeLabel.setFont(new Font("Monospaced", Font.BOLD, 13));
        modeLabel.setForeground(DIM);
        modeLabel.setBorder(BorderFactory.createCompoundBorder(
            new LineBorder(DIM, 1, true),
            new EmptyBorder(3, 14, 3, 14)));
        modeRow.add(modeLabel);
        topWrapper.add(modeRow);
        topWrapper.add(Box.createVerticalStrut(6));

        // Connect + status row
        JPanel topBar = new JPanel(new FlowLayout(FlowLayout.CENTER, 12, 0));
        topBar.setOpaque(false);
        tcpConnBtn = mkBtn("▶ CONNECT TELEMETRY", GREEN);
        tcpConnBtn.setPreferredSize(new Dimension(200, 30));
        tcpConnBtn.addActionListener(e -> toggleTcp());
        tcpStatus = new JLabel("Disconnected");
        tcpStatus.setFont(F_LABEL); tcpStatus.setForeground(DIM);
        topBar.add(tcpConnBtn); topBar.add(tcpStatus);
        topWrapper.add(topBar);

        p.add(topWrapper, BorderLayout.NORTH);

        // ── Center: sensor card ───────────────────────────────────────
        JPanel card = new JPanel();
        card.setLayout(new BoxLayout(card, BoxLayout.Y_AXIS));
        card.setBackground(PANEL_BG);
        card.setBorder(BorderFactory.createCompoundBorder(
            new LineBorder(new Color(0x28, 0x28, 0x35), 1, true),
            new EmptyBorder(18, 28, 18, 28)));

        // Ultrasonic bars (max 400 cm)
        usBar1 = new SensorBar("US1  (Front)", ACCENT, 400);
        usBar2 = new SensorBar("US2  (Left) ", ACCENT, 400);
        usBar3 = new SensorBar("US3  (Right)", ACCENT, 400);
        card.add(usBar1); card.add(Box.createVerticalStrut(12));
        card.add(usBar2); card.add(Box.createVerticalStrut(12));
        card.add(usBar3); card.add(Box.createVerticalStrut(22));

        // IR row
        JPanel irRow = new JPanel(new FlowLayout(FlowLayout.CENTER, 40, 0));
        irRow.setOpaque(false);
        irLeft  = new IrLight("IR Left");
        irRight = new IrLight("IR Right");
        irRow.add(irLeft); irRow.add(irRight);
        card.add(irRow);

        p.add(card, BorderLayout.CENTER);
        return p;
    }

    // ── TCP toggle ────────────────────────────────────────────────────
    private void toggleTcp() {
        if (tcpClient != null && tcpClient.isRunning()) {
            tcpClient.stop();
            tcpClient = null;
            tcpConnBtn.setForeground(GREEN);
            tcpConnBtn.setText("▶ CONNECT TELEMETRY");
            tcpStatus.setText("Disconnected"); tcpStatus.setForeground(DIM);
        } else {
            String ip = ipField.getText().trim();
            tcpStatus.setText("Connecting..."); tcpStatus.setForeground(WARN);
            tcpConnBtn.setForeground(DIM);
            tcpClient = new TcpClient(ip, 81, this::onTcpMessage, this::onTcpState);
            tcpClient.start();
        }
    }

    // Called on EDT via SwingUtilities.invokeLater inside TcpClient
    void onTcpMessage(String json) {
        // Parse {"US1":45,"US2":120,"US3":9999,"IR1":0,"IR2":1,"MODE":"SUMO"}
        int us1 = jsonInt(json, "US1");
        int us2 = jsonInt(json, "US2");
        int us3 = jsonInt(json, "US3");
        int ir1 = jsonInt(json, "IR1");
        int ir2 = jsonInt(json, "IR2");

        // Extract MODE string
        String newMode = jsonStr(json, "MODE");
        if (newMode != null && !newMode.equals(currentMode)) {
            currentMode = newMode;
            updateModeBadge(currentMode);
        }

        usBar1.setValue(us1);
        usBar2.setValue(us2);
        usBar3.setValue(us3);

        // IR interpretation differs by mode:
        //   SUMO : HIGH (1) = white edge detected
        //   LINE : LOW  (0) = on black line (normal)
        //   Others: show raw HIGH/LOW as edge indicator (HIGH=1=edge)
        boolean ir1Edge, ir2Edge;
        if ("LINE".equals(currentMode)) {
            // For line follower: LOW (0) = on line (normal/safe), HIGH (1) = off line
            ir1Edge = (ir1 == 1);   // off the line
            ir2Edge = (ir2 == 1);
            irLeft.setMode("LINE");
            irRight.setMode("LINE");
        } else {
            // Sumo / Maze / default: HIGH (1) = white edge
            ir1Edge = (ir1 == 1);
            ir2Edge = (ir2 == 1);
            irLeft.setMode("SUMO");
            irRight.setMode("SUMO");
        }
        irLeft.setState(ir1Edge);
        irRight.setState(ir2Edge);
    }

    void onTcpState(boolean connected) {
        if (connected) {
            tcpStatus.setText("● Connected");   tcpStatus.setForeground(GREEN);
            tcpConnBtn.setText("■ DISCONNECT"); tcpConnBtn.setForeground(RED);
        } else {
            tcpStatus.setText("✗ Lost connection"); tcpStatus.setForeground(RED);
            tcpConnBtn.setText("▶ CONNECT TELEMETRY"); tcpConnBtn.setForeground(GREEN);
            tcpClient = null;
        }
    }

    // ══════════════════════════════════════════════════════════════════
    //  OTA TAB  (tab 1)
    // ══════════════════════════════════════════════════════════════════
    private JPanel buildOtaTab() {
        JPanel p = new JPanel(new GridBagLayout());
        p.setBackground(BG);
        p.setBorder(new EmptyBorder(14, 14, 14, 14));

        JPanel card = new JPanel();
        card.setLayout(new BoxLayout(card, BoxLayout.Y_AXIS));
        card.setBackground(PANEL_BG);
        card.setBorder(BorderFactory.createCompoundBorder(
            new LineBorder(new Color(0x28, 0x28, 0x35), 1, true),
            new EmptyBorder(18, 24, 18, 24)));

        // Info
        JLabel info = new JLabel(
            "<html><div style='text-align:center;font-family:monospace'>" +
            "Connect ESP32 to <b style='color:#00E5FF'>FullyPaid</b>, " +
            "then check Serial Monitor for its IP.</div></html>");
        info.setFont(F_SMALL); info.setForeground(TEXT);
        info.setAlignmentX(CENTER_ALIGNMENT);
        card.add(info);
        card.add(Box.createVerticalStrut(18));

        // File pick row
        JPanel fileRow = new JPanel(new FlowLayout(FlowLayout.CENTER, 10, 0));
        fileRow.setOpaque(false); fileRow.setAlignmentX(CENTER_ALIGNMENT);
        pickBtn = mkBtn("📂 SELECT .BIN", ACCENT);
        pickBtn.setPreferredSize(new Dimension(160, 28));
        pickBtn.addActionListener(e -> pickFile());
        fileLabel = new JLabel("No file selected");
        fileLabel.setFont(F_LABEL); fileLabel.setForeground(DIM);
        fileRow.add(pickBtn); fileRow.add(fileLabel);
        card.add(fileRow);
        card.add(Box.createVerticalStrut(18));

        // Upload button
        uploadBtn = mkBtn("⬆ UPLOAD FIRMWARE", ACCENT2);
        uploadBtn.setPreferredSize(new Dimension(220, 36));
        uploadBtn.setMaximumSize(new Dimension(220, 36));
        uploadBtn.setAlignmentX(CENTER_ALIGNMENT);
        uploadBtn.setEnabled(false);
        uploadBtn.addActionListener(e -> doUpload());
        card.add(uploadBtn);
        card.add(Box.createVerticalStrut(16));

        // Progress
        progress = new JProgressBar(0, 100);
        progress.setStringPainted(true); progress.setFont(F_LABEL);
        progress.setForeground(ACCENT); progress.setBackground(BTN_BG);
        progress.setBorder(new LineBorder(DIM, 1));
        progress.setMaximumSize(new Dimension(Integer.MAX_VALUE, 28));
        progress.setAlignmentX(CENTER_ALIGNMENT);
        card.add(progress);
        card.add(Box.createVerticalStrut(10));

        // Status
        uploadStatus = new JLabel("Waiting...", SwingConstants.CENTER);
        uploadStatus.setFont(F_LABEL); uploadStatus.setForeground(DIM);
        uploadStatus.setAlignmentX(CENTER_ALIGNMENT);
        card.add(uploadStatus);
        card.add(Box.createVerticalGlue());

        GridBagConstraints c = new GridBagConstraints();
        c.fill = GridBagConstraints.BOTH; c.weightx = 1; c.weighty = 1;
        p.add(card, c);
        return p;
    }

    // Tiny JSON int extractor — no external library
    private static int jsonInt(String json, String key) {
        String token = "\"" + key + "\":";
        int i = json.indexOf(token);
        if (i < 0) return -1;
        i += token.length();
        int j = i;
        while (j < json.length() &&
               (Character.isDigit(json.charAt(j)) || json.charAt(j) == '-')) j++;
        try { return Integer.parseInt(json.substring(i, j)); } catch (Exception e) { return -1; }
    }

    // Tiny JSON string extractor  — returns value of a string field, or null
    private static String jsonStr(String json, String key) {
        String token = "\"" + key + "\":\"";
        int i = json.indexOf(token);
        if (i < 0) return null;
        i += token.length();
        int j = json.indexOf('"', i);
        if (j < 0) return null;
        return json.substring(i, j);
    }

    // Update the mode badge label & colour
    private void updateModeBadge(String mode) {
        String label = " ◈  MODE:  " + mode + "  ◈ ";
        Color c;
        switch (mode) {
            case "SUMO":   c = ACCENT2; break;
            case "HOCKEY": c = WARN;    break;
            case "MAZE":   c = ACCENT;  break;
            case "LINE":   c = GREEN;   break;
            default:       c = DIM;     break;
        }
        modeLabel.setText(label);
        modeLabel.setForeground(c);
        modeLabel.setBorder(BorderFactory.createCompoundBorder(
            new LineBorder(c, 1, true),
            new EmptyBorder(3, 14, 3, 14)));
    }

    // ── Ping ──────────────────────────────────────────────────────────
    private void doPing() {
        String ip = ipField.getText().trim();
        pingStatus.setText("Pinging..."); pingStatus.setForeground(WARN);
        new Thread(() -> {
            boolean ok = false;
            try {
                HttpURLConnection c =
                    (HttpURLConnection) new URL("http://" + ip + "/").openConnection();
                c.setConnectTimeout(2500); c.setReadTimeout(2500);
                c.setRequestMethod("GET");
                ok = c.getResponseCode() < 500;
                c.disconnect();
            } catch (Exception ignored) {}
            final boolean r = ok;
            SwingUtilities.invokeLater(() -> {
                if (r) { pingStatus.setText("● Reachable");   pingStatus.setForeground(GREEN); }
                else   { pingStatus.setText("✗ No response"); pingStatus.setForeground(RED);   }
            });
        }).start();
    }

    // ── Pick .bin ─────────────────────────────────────────────────────
    private void pickFile() {
        JFileChooser fc = new JFileChooser();
        fc.setFileFilter(
            new javax.swing.filechooser.FileNameExtensionFilter("Firmware (*.bin)", "bin"));
        if (fc.showOpenDialog(this) == JFileChooser.APPROVE_OPTION) {
            selectedBin = fc.getSelectedFile();
            fileLabel.setText(selectedBin.getName() +
                              "  (" + (selectedBin.length() / 1024) + " KB)");
            fileLabel.setForeground(ACCENT);
            uploadBtn.setEnabled(true);
        }
    }

    // ── Upload firmware ───────────────────────────────────────────────
    private void doUpload() {
        if (selectedBin == null || !selectedBin.exists()) return;
        String ip = ipField.getText().trim();
        uploadBtn.setEnabled(false); pickBtn.setEnabled(false);
        progress.setValue(0);
        uploadStatus.setText("Uploading..."); uploadStatus.setForeground(WARN);

        new Thread(() -> {
            try {
                String boundary = "----OTABoundary" + System.currentTimeMillis();
                HttpURLConnection conn =
                    (HttpURLConnection) new URL("http://" + ip + "/update").openConnection();
                conn.setDoOutput(true); conn.setRequestMethod("POST");
                conn.setConnectTimeout(5000); conn.setReadTimeout(60000);
                conn.setRequestProperty("Content-Type",
                    "multipart/form-data; boundary=" + boundary);

                byte[] header = ("--" + boundary + "\r\n" +
                    "Content-Disposition: form-data; name=\"update\";" +
                    " filename=\"firmware.bin\"\r\n" +
                    "Content-Type: application/octet-stream\r\n\r\n").getBytes();
                byte[] footer = ("\r\n--" + boundary + "--\r\n").getBytes();
                long total = header.length + selectedBin.length() + footer.length;
                conn.setFixedLengthStreamingMode(total);

                try (OutputStream out = conn.getOutputStream();
                     FileInputStream fis = new FileInputStream(selectedBin)) {
                    out.write(header);
                    byte[] buf = new byte[4096];
                    long sent = header.length; int n;
                    while ((n = fis.read(buf)) != -1) {
                        out.write(buf, 0, n); sent += n;
                        final int pct = (int)((sent * 100L) / total);
                        SwingUtilities.invokeLater(() -> {
                            progress.setValue(pct);
                            progress.setString(pct + "%");
                        });
                    }
                    out.write(footer);
                }
                int code = conn.getResponseCode();
                InputStream rs = (code >= 200 && code < 300)
                    ? conn.getInputStream() : conn.getErrorStream();
                String resp = (rs != null)
                    ? new java.util.Scanner(rs).useDelimiter("\\A").next() : "";
                conn.disconnect();
                final boolean ok = resp.trim().toLowerCase().startsWith("ok");
                SwingUtilities.invokeLater(() -> {
                    progress.setValue(100);
                    if (ok) {
                        uploadStatus.setText("✔ Done — ESP32 rebooting...");
                        uploadStatus.setForeground(GREEN);
                    } else {
                        uploadStatus.setText("✗ Failed: " + resp.trim());
                        uploadStatus.setForeground(RED);
                    }
                    uploadBtn.setEnabled(true); pickBtn.setEnabled(true);
                });
            } catch (Exception ex) {
                SwingUtilities.invokeLater(() -> {
                    uploadStatus.setText("Error: " + ex.getMessage());
                    uploadStatus.setForeground(RED);
                    uploadBtn.setEnabled(true); pickBtn.setEnabled(true);
                });
            }
        }).start();
    }

    // ── Button factory ────────────────────────────────────────────────
    static JButton mkBtn(String text, Color fg) {
        JButton b = new JButton(text) {
            @Override protected void paintComponent(Graphics g) {
                Graphics2D g2 = (Graphics2D) g.create();
                g2.setRenderingHint(RenderingHints.KEY_ANTIALIASING,
                                    RenderingHints.VALUE_ANTIALIAS_ON);
                g2.setColor(getModel().isRollover() ? BTN_HOV : BTN_BG);
                g2.fillRoundRect(0, 0, getWidth(), getHeight(), 8, 8);
                g2.setColor(getForeground()); g2.setFont(getFont());
                FontMetrics fm = g2.getFontMetrics();
                g2.drawString(getText(),
                    (getWidth()  - fm.stringWidth(getText())) / 2,
                    (getHeight() + fm.getAscent() - fm.getDescent()) / 2);
                g2.dispose();
            }
        };
        b.setFont(F_SMALL); b.setForeground(fg);
        b.setOpaque(false); b.setContentAreaFilled(false);
        b.setBorderPainted(false); b.setFocusPainted(false);
        b.setCursor(Cursor.getPredefinedCursor(Cursor.HAND_CURSOR));
        return b;
    }

    public static void main(String[] args) {
        try { UIManager.setLookAndFeel(UIManager.getCrossPlatformLookAndFeelClassName()); }
        catch (Exception ignored) {}
        SwingUtilities.invokeLater(() -> new OtaGUI().setVisible(true));
    }


    // ══════════════════════════════════════════════════════════════════
    //  SENSOR BAR  — label + progress bar + cm value
    // ══════════════════════════════════════════════════════════════════
    static class SensorBar extends JPanel {
        private final JProgressBar bar;
        private final JLabel       valLbl;
        private final int          maxCm;

        SensorBar(String name, Color clr, int maxCm) {
            this.maxCm = maxCm;
            setOpaque(false);
            setLayout(new BorderLayout(8, 0));

            JLabel lbl = new JLabel(name);
            lbl.setFont(F_LABEL); lbl.setForeground(DIM);
            lbl.setPreferredSize(new Dimension(130, 20));
            add(lbl, BorderLayout.WEST);

            bar = new JProgressBar(0, maxCm);
            bar.setStringPainted(false);
            bar.setForeground(clr); bar.setBackground(BTN_BG);
            bar.setBorder(new LineBorder(new Color(0x28, 0x28, 0x35), 1));
            bar.setPreferredSize(new Dimension(200, 18));
            add(bar, BorderLayout.CENTER);

            valLbl = new JLabel("---  cm", SwingConstants.RIGHT);
            valLbl.setFont(F_LABEL); valLbl.setForeground(TEXT);
            valLbl.setPreferredSize(new Dimension(80, 20));
            add(valLbl, BorderLayout.EAST);
        }

        void setValue(int cm) {
            if (cm <= 0 || cm >= 9999) {
                bar.setValue(0);
                valLbl.setText("OOR");
                valLbl.setForeground(DIM);
            } else {
                int clamped = Math.min(cm, maxCm);
                bar.setValue(clamped);
                valLbl.setText(cm + " cm");
                Color c = cm < 20 ? RED : (cm < 60 ? WARN : GREEN);
                bar.setForeground(c);
                valLbl.setForeground(c);
            }
        }
    }


    // ══════════════════════════════════════════════════════════════════
    //  IR LIGHT  — circular indicator + label
    // ══════════════════════════════════════════════════════════════════
    static class IrLight extends JPanel {
        private boolean edgeDetected = false;
        private String  irMode = "SUMO";   // "SUMO" or "LINE"
        private final JLabel lbl;

        IrLight(String name) {
            setOpaque(false);
            setLayout(new BoxLayout(this, BoxLayout.Y_AXIS));
            setPreferredSize(new Dimension(90, 80));

            JPanel circle = new JPanel() {
                @Override protected void paintComponent(Graphics g) {
                    super.paintComponent(g);
                    Graphics2D g2 = (Graphics2D) g.create();
                    g2.setRenderingHint(RenderingHints.KEY_ANTIALIASING,
                                        RenderingHints.VALUE_ANTIALIAS_ON);
                    Color fill = edgeDetected ? RED : GREEN;
                    g2.setColor(fill.darker().darker());
                    g2.fillOval(4, 4, 42, 42);
                    g2.setColor(fill);
                    g2.fillOval(8, 8, 34, 34);
                    g2.setColor(new Color(255, 255, 255, 60));
                    g2.fillOval(14, 14, 12, 12);
                    g2.dispose();
                }
            };
            circle.setOpaque(false);
            circle.setPreferredSize(new Dimension(50, 50));
            circle.setMaximumSize(new Dimension(50, 50));
            circle.setAlignmentX(CENTER_ALIGNMENT);

            lbl = new JLabel(name, SwingConstants.CENTER);
            lbl.setFont(F_SMALL); lbl.setForeground(DIM);
            lbl.setAlignmentX(CENTER_ALIGNMENT);

            JLabel stateLbl = new JLabel("SAFE", SwingConstants.CENTER);
            stateLbl.setName("state");
            stateLbl.setFont(F_LABEL); stateLbl.setForeground(GREEN);
            stateLbl.setAlignmentX(CENTER_ALIGNMENT);

            add(circle);
            add(Box.createVerticalStrut(2));
            add(lbl);
            add(stateLbl);
        }

        void setMode(String mode) { irMode = mode; }

        void setState(boolean triggered) {
            edgeDetected = triggered;
            for (Component c : getComponents()) {
                if (c instanceof JLabel && "state".equals(c.getName())) {
                    JLabel sl = (JLabel) c;
                    if ("LINE".equals(irMode)) {
                        // triggered = off the line
                        if (triggered) { sl.setText("OFF LINE"); sl.setForeground(RED);   }
                        else           { sl.setText("ON LINE");  sl.setForeground(GREEN); }
                    } else {
                        // triggered = white edge (Sumo/default)
                        if (triggered) { sl.setText("EDGE!");   sl.setForeground(RED);   }
                        else           { sl.setText("SAFE");    sl.setForeground(GREEN); }
                    }
                }
            }
            repaint();
        }
    }


    // ══════════════════════════════════════════════════════════════════
    //  PLAIN TCP CLIENT
    //  Connects to the ESP32 TCP server (port 81).
    //  The ESP32 sends one JSON line per sensor reading, terminated by '\n'.
    //  No HTTP upgrade, no framing — just raw newline-delimited text.
    // ══════════════════════════════════════════════════════════════════
    static class TcpClient {
        interface MessageHandler { void onMessage(String msg); }
        interface StateHandler   { void onState(boolean connected); }

        private final String         host;
        private final int            port;
        private final MessageHandler onMsg;
        private final StateHandler   onState;
        private final AtomicBoolean  running = new AtomicBoolean(false);
        private Thread thread;
        private Socket socket;

        TcpClient(String host, int port, MessageHandler onMsg, StateHandler onState) {
            this.host = host; this.port = port;
            this.onMsg = onMsg; this.onState = onState;
        }

        boolean isRunning() { return running.get(); }

        void start() {
            running.set(true);
            thread = new Thread(this::run, "TcpClient");
            thread.setDaemon(true);
            thread.start();
        }

        void stop() {
            running.set(false);
            try { if (socket != null) socket.close(); } catch (Exception ignored) {}
        }

        private void run() {
            try {
                socket = new Socket();
                socket.connect(new InetSocketAddress(host, port), 3000);
                socket.setSoTimeout(3000);   // read timeout — keeps loop responsive

                notifyState(true);

                BufferedReader reader = new BufferedReader(
                    new InputStreamReader(socket.getInputStream(),
                                          java.nio.charset.StandardCharsets.UTF_8));

                while (running.get()) {
                    try {
                        String line = reader.readLine();  // blocks until '\n' or timeout
                        if (line == null) break;          // server closed connection
                        line = line.trim();
                        if (!line.isEmpty()) {
                            final String msg = line;
                            SwingUtilities.invokeLater(() -> onMsg.onMessage(msg));
                        }
                    } catch (java.net.SocketTimeoutException ignored) {
                        // just loop; keeps the thread responsive to stop()
                    }
                }
            } catch (Exception e) {
                // connection failed or dropped — fall through to finally
            } finally {
                running.set(false);
                try { if (socket != null) socket.close(); } catch (Exception ignored) {}
                SwingUtilities.invokeLater(() -> onState.onState(false));
            }
        }

        private void notifyState(boolean connected) {
            SwingUtilities.invokeLater(() -> onState.onState(connected));
        }
    }
}
