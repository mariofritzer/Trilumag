"""Bauteillisten der beiden Trilumag-Platinen (Rev 0.2, Handlöt-Ausführung)."""
from gen import Design
from board import ept, erot, toward, A, B, C, G, H, L, xform, P

R0805 = 'Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder'
R1206 = 'Resistor_SMD:R_1206_3216Metric_Pad1.30x1.75mm_HandSolder'
R2512 = 'Resistor_SMD:R_2512_6332Metric_Pad1.40x3.35mm_HandSolder'
C0805 = 'Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder'
C1206 = 'Capacitor_SMD:C_1206_3216Metric_Pad1.33x1.80mm_HandSolder'
HOLE = 'MountingHole:MountingHole_3.2mm_M3_Pad'


def R(d, ref, val, n1, n2, near, fp=R0805, grp='', mpn=''):
    d.add(ref, 'Device', 'R', val, fp, {'1': n1, '2': n2}, group=grp, near=near, mpn=mpn)


def Cap(d, ref, val, n1, n2, near, fp=C0805, grp='', mpn=''):
    d.add(ref, 'Device', 'C', val, fp, {'1': n1, '2': n2}, group=grp, near=near, mpn=mpn)


def edges_and_strips(d, pogo_edges, sns):
    """Pogo-Stecker/-Buchse an der Kantenmitte ±15 mm, WS2814-Lötpads je Kante, Datenkette 1→2→3."""
    for k in pogo_edges:
        r = erot(k)
        pins = {'1': 'GND', '2': 'A', '3': 'B', '4': sns[k], '5': 'GND', '6': '+24V'}
        d.add(f'X{k}A', 'Connector_Generic', 'Conn_01x06', 'Pogo 6P Stecker (Kabel)', 'Trilumag:Kabel_6P_Stecker',
              pins, at=ept(k, 116, 4), rot=r, group='Kanten', mpn='Magnet-Pogo 6-pol. männlich',
              note='Kabel zum Magnet-Pogo-Stecker; Pin 1 und 6 sind die dicken Kontakte')
        d.add(f'X{k}B', 'Connector_Generic', 'Conn_01x06', 'Pogo 6P Buchse (Kabel)', 'Trilumag:Kabel_6P_Buchse',
              pins, at=ept(k, 84, 4), rot=r, group='Kanten', mpn='Magnet-Pogo 6-pol. weiblich',
              note='Kabel zur Magnet-Pogo-Buchse; Pin 1 und 6 sind die dicken Kontakte')
    chain = {1: ('LED_D1', 'GND', 'LED_D2', 'LED_B2'), 2: ('LED_D2', 'LED_B2', 'LED_D3', 'LED_B3'), 3: ('LED_D3', 'LED_B3', None, None)}
    for k in (1, 2, 3):
        di, bi, do, bo = chain[k]
        r = erot(k)
        d.add(f'L{k}S', 'Connector_Generic', 'Conn_01x04', f'Streifen {k} Anfang', 'Trilumag:WS2814_Anfang',
              {'1': '+24V', '2': di, '3': bi, '4': 'GND'}, at=ept(k, 48, 13), rot=r, group='LED-Streifen')
        e = {'1': '+24V', '4': 'GND'}
        if do:
            e.update({'2': do, '3': bo})
        d.add(f'L{k}E', 'Connector_Generic', 'Conn_01x04', f'Streifen {k} Ende', 'Trilumag:WS2814_Ende',
              e, at=ept(k, 152, 13), rot=r, group='LED-Streifen')


def holes(dist=(45, 45, 45)):
    """M3-Befestigungslöcher auf den Winkelhalbierenden, dist mm vom Schwerpunkt."""
    return {f'H{i + 1}': (HOLE, toward(v, dist[i])) for i, v in enumerate((A, B, C))}


CLASSES = [('Power', 2.0, 0.2, 1.2, 0.6), ('Masse', 0.5, 0.2, 0.8, 0.4)]


# --------------------------------------------------------------------------------------------
def panel():
    d = Design('trilumag-panel', 'Trilumag Panel (Nebenplatine)')
    gx, gy = G
    g = 'Versorgung'
    d.add('U3', 'Regulator_Linear', 'L78L05_SOT89', '78L05', 'Package_TO_SOT_SMD:SOT-89-3_Handsoldering',
          {'3': 'V24_LDO', '1': '+5V', '2': 'GND'}, at=(gx + 16, gy - 8), rot=0, group=g, mpn='78L05 SOT-89 (≥30 V Eingang)')
    d.add('D1', 'Device', 'D_TVS', 'SMBJ26A', 'Diode_SMD:D_SMB_Handsoldering', {'1': '+24V', '2': 'GND'},
          near=('U3', '3'), group=g, mpn='SMBJ26A')
    Cap(d, 'C1', '10µF 50V', '+24V', 'GND', ('D1', '1'), C1206, g, 'X5R/X7R 1206 50 V')
    R(d, 'R8', '10Ω', '+24V', 'V24_LDO', ('U3', '3'), R1206, g)
    Cap(d, 'C2', '1µF 50V', 'V24_LDO', 'GND', ('U3', '3'), C0805, g)
    Cap(d, 'C3', '1µF', '+5V', 'GND', ('U3', '1'), C0805, g)
    d.add('U4', 'Regulator_Linear', 'XC6206PxxxMR', 'XC6206P332MR', 'Package_TO_SOT_SMD:SOT-23_Handsoldering',
          {'3': '+5V', '2': '+3V3', '1': 'GND'}, near=('U3', '1'), group=g, mpn='XC6206P332MR (3,3 V)')
    Cap(d, 'C6', '1µF', '+5V', 'GND', ('U4', '3'), C0805, g)
    Cap(d, 'C7', '1µF', '+3V3', 'GND', ('U4', '2'), C0805, g)

    g = 'Mikrocontroller'
    d.add('U1', 'MCU_WCH_CH32V0', 'CH32V003FxPx', 'CH32V003F4P6', 'Package_SO:TSSOP-20_4.4x6.5mm_P0.65mm',
          {'1': 'DE', '2': 'TX', '3': 'RX', '7': 'GND', '9': '+3V3', '10': 'S3R', '11': 'SDA', '12': 'SCL',
           '13': 'S1R', '14': 'S2R', '16': 'LED_IN', '18': 'SWIO', '19': 'TINT'}, at=(gx, gy), rot=0, group=g, mpn='CH32V003F4P6')
    Cap(d, 'C4', '100nF', '+3V3', 'GND', ('U1', '9'), C0805, g)
    for i, (pin, net) in enumerate([('13', '1'), ('14', '2'), ('10', '3')]):
        R(d, f'R{i + 1}', '100Ω', f'S{net}R', f'SNS{net}', ('U1', pin), R0805, g)
    d.add('J5', 'Connector_Generic', 'Conn_01x03', 'SWD', 'Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical',
          {'1': 'SWIO', '2': 'GND', '3': '+3V3'}, at=(gx - 20, gy + 2), rot=0, group=g, note='WCH-LinkE: SWDIO, GND, 3V3')

    g = 'RS-485'
    d.add('U2', 'Interface_UART', 'MAX3485', 'MAX3485', 'Package_SO:SOIC-8_3.9x4.9mm_P1.27mm',
          {'1': 'RX', '2': 'DE', '3': 'DE', '4': 'TX', '5': 'GND', '6': 'A', '7': 'B', '8': '+3V3'},
          at=(gx - 14, gy - 12), rot=0, group=g, mpn='MAX3485ESA / SP3485EN')
    Cap(d, 'C5', '100nF', '+3V3', 'GND', ('U2', '8'), C0805, g)

    g = 'Lagesensor'
    jat = (gx + 8.89, gy + 28)
    d.add('J3', 'Connector_Generic', 'Conn_01x08', 'LIS3DH-Modul', 'Connector_PinSocket_2.54mm:PinSocket_1x08_P2.54mm_Vertical',
          {'1': '+3V3', '2': 'GND', '3': 'SCL', '4': 'SDA', '5': 'GND', '6': '+3V3', '7': 'TINT'}, at=jat, rot=-90, group=g,
          note='Pinfolge VCC GND SCL SDA SDO CS INT1 INT2 – mit dem Modul vergleichen')
    R(d, 'R6', '4.7k', 'SDA', '+3V3', ('U1', '11'), R0805, g)
    R(d, 'R7', '4.7k', 'SCL', '+3V3', ('U1', '12'), R0805, g)
    Cap(d, 'C8', '100nF', '+3V3', 'GND', ('J3', '1'), C0805, g)

    g = 'LED-Daten'
    d.add('U6', '74xGxx', '74AHCT1G125', '74AHCT1G125', 'Package_TO_SOT_SMD:SOT-23-5_HandSoldering',
          {'1': 'GND', '2': 'LED_IN', '3': 'GND', '4': 'LED_Y', '5': '+5V'}, near=('U1', '16'), group=g, mpn='74AHCT1G125 SOT-23-5')
    Cap(d, 'C9', '100nF', '+5V', 'GND', ('U6', '5'), C0805, g)
    R(d, 'R4', '33Ω', 'LED_Y', 'LED_D1', ('U6', '4'), R0805, g)
    R(d, 'R5', '10k', 'LED_IN', 'GND', ('U6', '2'), R0805, g)

    edges_and_strips(d, (1, 2, 3), {1: 'SNS1', 2: 'SNS2', 3: 'SNS3'})
    # Platz für das aufgesteckte Sensormodul (ca. 21 × 16 mm) freihalten
    mod = xform([(-15.5, -1.5), (1.5, -1.5), (1.5, 19.3), (-15.5, 19.3)], jat, -90)
    cfg = dict(classes=CLASSES, holes=holes(), reserve=[mod],
               classmap={'Power': ['+24V'], 'Masse': ['GND']}, ring='closed',
               texts=[('TRILUMAG · Panel · Rev 0.2', (gx, gy - 30), 2.0), ('Sensormodul (steckt hier, Bauteile darunter frei)', (gx, gy + 20), 1.0)])
    return d, cfg


# --------------------------------------------------------------------------------------------
def main():
    d = Design('trilumag-hauptplatine', 'Trilumag Hauptplatine (ESP32-S3)')
    gx, gy = G
    g = 'Eingang & Schutz'
    d.add('J1', 'Connector', 'Barrel_Jack', '24V DC 5.5/2.1', 'Connector_BarrelJack:BarrelJack_Horizontal',
          {'1': 'VIN', '2': 'GND'}, padnets={'3': 'GND'}, at=P(26, H - 13.7), rot=90, group=g, mpn='DC-Buchse 5,5/2,1 mm, ≥5 A')
    d.add('F1', 'Device', 'Fuse', '6.3A träge', 'Fuse:Fuseholder_Clip-5x20mm_Bel_FC-203-22_Lateral_P17.80x5.00mm_D1.17mm_Horizontal',
          {'1': 'VIN', '2': 'VIN_F'}, at=P(68, 145), rot=0, group=g, mpn='Sicherungshalter 5×20 + Feinsicherung T6,3A')
    d.add('Q1', 'Device', 'Q_PMOS_GDS', 'AOD4185', 'Package_TO_SOT_SMD:TO-252-2',
          {'1': 'QG', '2': 'VIN_F', '3': '+24V_ROH'}, near=('F1', '2'), rot=0, group=g, mpn='AOD4185 (P-MOSFET −40 V)')
    d.add('D2', 'Device', 'D_Zener', '12V', 'Diode_SMD:D_SOD-123', {'1': '+24V_ROH', '2': 'QG'}, near=('Q1', '1'), group=g, mpn='BZT52C12')
    R(d, 'R1', '100k', 'QG', 'GND', ('Q1', '1'), R0805, g)
    d.add('D1', 'Device', 'D_TVS', 'SMBJ26A', 'Diode_SMD:D_SMB_Handsoldering', {'1': '+24V_ROH', '2': 'GND'}, near=('Q1', '3'), group=g, mpn='SMBJ26A')
    d.add('C1', 'Device', 'C_Polarized', '470µF 35V', 'Capacitor_SMD:CP_Elec_10x10.5', {'1': '+24V_ROH', '2': 'GND'},
          near=('Q1', '3'), group=g, mpn='Elko SMD 10×10,5 470 µF 35 V')

    g = 'Strommessung'
    d.add('R14', 'Device', 'R', '5mΩ', R2512, {'1': '+24V_ROH', '2': '+24V'}, near=('C1', '1'), rot=0, group=g, mpn='Shunt 5 mΩ 2512 2 W')
    d.add('U6', 'Sensor_Energy', 'INA226', 'INA226', 'Package_SO:VSSOP-10_3x3mm_P0.5mm',
          {'10': 'SHUNT_P', '9': 'SHUNT_N', '8': 'SHUNT_N', '6': '+3V3', '7': 'GND', '1': 'GND', '2': 'GND', '4': 'SDA', '5': 'SCL'},
          near=('R14', '1'), group=g, mpn='INA226AIDGSR (I²C 0x40)')
    R(d, 'R17', '10Ω', '+24V_ROH', 'SHUNT_P', ('U6', '10'), R0805, g)
    R(d, 'R18', '10Ω', '+24V', 'SHUNT_N', ('U6', '9'), R0805, g)
    Cap(d, 'C13', '100nF', 'SHUNT_P', 'SHUNT_N', ('U6', '10'), C0805, g)
    Cap(d, 'C12', '100nF', '+3V3', 'GND', ('U6', '6'), C0805, g)

    g = 'Versorgung'
    d.add('PS1', 'Regulator_Switching', 'R-783.3-1.0', 'R-78E3.3-1.0', 'Converter_DCDC:Converter_DCDC_RECOM_R-78E-0.5_THT',
          {'1': '+24V', '2': 'GND', '3': '+3V3'}, at=(gx - 10, gy + 4), rot=0, group=g, mpn='RECOM R-78E3.3-1.0 (oder R-783.3-1.0)')
    Cap(d, 'C2', '10µF 50V', '+24V', 'GND', ('PS1', '1'), C1206, g)
    Cap(d, 'C3', '22µF', '+3V3', 'GND', ('PS1', '3'), C1206, g)
    d.add('D3', 'Device', 'D_Schottky', 'SS34', 'Diode_SMD:D_SMA_Handsoldering', {'1': '+24V', '2': 'VBUS'}, near=('PS1', '1'), group=g,
          mpn='SS34', note='nur USB angesteckt: Platine läuft zum Flashen über USB')
    R(d, 'R19', '10Ω', '+24V', 'V24_LDO', ('PS1', '1'), R1206, g)
    d.add('U2', 'Regulator_Linear', 'L78L05_SOT89', '78L05', 'Package_TO_SOT_SMD:SOT-89-3_Handsoldering',
          {'3': 'V24_LDO', '1': '+5V', '2': 'GND'}, near=('PS1', '1'), group=g)
    Cap(d, 'C9', '1µF 50V', 'V24_LDO', 'GND', ('U2', '3'), C0805, g)
    Cap(d, 'C10', '1µF', '+5V', 'GND', ('U2', '1'), C0805, g)

    g = 'ESP32-S3'
    d.add('U3', 'RF_Module', 'ESP32-S3-WROOM-1', 'ESP32-S3-WROOM-1-N8R2', 'RF_Module:ESP32-S3-WROOM-1',
          {'GND': 'GND', '3V3': '+3V3', 'EN': 'EN', 'IO4': 'SNS_R_IO', 'IO5': 'SNS_L_IO', 'IO16': 'LED_3V3', 'IO17': 'TX', 'IO18': 'RX',
           'IO8': 'DE', 'IO19': 'USB_DN', 'IO20': 'USB_DP', 'IO0': 'BOOT', 'IO1': 'SDA', 'IO2': 'SCL'},
          at=P(100, 88.0), rot=0, group=g, mpn='ESP32-S3-WROOM-1 (N8R2 oder N16R8)')
    Cap(d, 'C4', '10µF', '+3V3', 'GND', ('U3', '2'), C0805, g)
    Cap(d, 'C5', '100nF', '+3V3', 'GND', ('U3', '2'), C0805, g)
    R(d, 'R4', '10k', 'EN', '+3V3', ('U3', '3'), R0805, g)
    Cap(d, 'C6', '1µF', 'EN', 'GND', ('U3', '3'), C0805, g)
    d.add('SW1', 'Switch', 'SW_Push', 'BOOT', 'Button_Switch_THT:SW_PUSH_6mm', {'1': 'BOOT', '2': 'GND'}, near=('U3', '27'), group=g)
    d.add('SW2', 'Switch', 'SW_Push', 'RESET', 'Button_Switch_THT:SW_PUSH_6mm', {'1': 'EN', '2': 'GND'}, near=('U3', '3'), group=g)
    d.add('J4', 'Connector', 'USB_C_Receptacle_USB2.0_16P', 'USB-C', 'Connector_USB:USB_C_Receptacle_GCT_USB4105-xx-A_16P_TopMnt_Horizontal',
          {'VBUS': 'VBUS', 'GND': 'GND', 'SHIELD': 'GND', 'CC1': 'CC1', 'CC2': 'CC2', 'D+': 'USB_DP', 'D-': 'USB_DN'},
          at=P(174, H - 3.73), rot=0, group=g, mpn='GCT USB4105-GF-A')
    R(d, 'R5', '5.1k', 'CC1', 'GND', ('J4', 'A5'), R0805, g)
    R(d, 'R6', '5.1k', 'CC2', 'GND', ('J4', 'B5'), R0805, g)
    R(d, 'R15', '4.7k', 'SDA', '+3V3', ('U3', '39'), R0805, g)
    R(d, 'R16', '4.7k', 'SCL', '+3V3', ('U3', '38'), R0805, g)

    g = 'RS-485'
    d.add('U4', 'Interface_UART', 'MAX3485', 'MAX3485', 'Package_SO:SOIC-8_3.9x4.9mm_P1.27mm',
          {'1': 'RX', '2': 'DE', '3': 'DE', '4': 'TX', '5': 'GND', '6': 'A', '7': 'B', '8': '+3V3'},
          at=(gx + 24, gy - 8), rot=0, group=g, mpn='MAX3485ESA / SP3485EN')
    Cap(d, 'C7', '100nF', '+3V3', 'GND', ('U4', '8'), C0805, g)
    R(d, 'R7', '120Ω', 'A', 'B', ('U4', '6'), R0805, g)
    R(d, 'R8', '680Ω', 'A', '+3V3', ('U4', '6'), R0805, g)
    R(d, 'R9', '680Ω', 'B', 'GND', ('U4', '7'), R0805, g)

    g = 'LED-Daten & Kanten'
    d.add('U5', '74xGxx', '74AHCT1G125', '74AHCT1G125', 'Package_TO_SOT_SMD:SOT-23-5_HandSoldering',
          {'1': 'GND', '2': 'LED_3V3', '3': 'GND', '4': 'LED_Y', '5': '+5V'}, at=(gx + 22, gy + 10), rot=0, group=g)
    Cap(d, 'C8', '100nF', '+5V', 'GND', ('U5', '5'), C0805, g)
    R(d, 'R10', '33Ω', 'LED_Y', 'LED_D1', ('U5', '4'), R0805, g)
    R(d, 'R11', '10k', 'LED_3V3', 'GND', ('U5', '2'), R0805, g)
    R(d, 'R12', '100Ω', 'SNS_R_IO', 'SNS_R', ('U3', '4'), R0805, g)
    R(d, 'R13', '100Ω', 'SNS_L_IO', 'SNS_L', ('U3', '5'), R0805, g)

    edges_and_strips(d, (2, 3), {2: 'SNS_R', 3: 'SNS_L'})
    # Antennen-Sperrfläche (beide Lagen, keine Kupferfläche/Leiterbahn/Via)
    ey = P(0, 88.0)[1]
    ant = [(gx - 12, ey - 12.8 - 8), (gx + 12, ey - 12.8 - 8), (gx + 12, ey - 6.7), (gx - 12, ey - 6.7)]
    cfg = dict(classes=CLASSES, holes=holes((45, 45, 85)), reserve=[ant[:]], keepouts=[ant],
               classmap={'Power': ['+24V', '+24V_ROH', 'VIN', 'VIN_F'], 'Masse': ['GND']}, ring='open',
               texts=[('TRILUMAG · Hauptplatine · Rev 0.2', P(100, 60), 2.0), ('24 V', P(26, H - 22), 1.5),
                      ('Antenne – Bereich frei lassen', P(100, 72), 1.0)])
    return d, cfg
