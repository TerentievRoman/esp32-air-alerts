<?php
/**
 * Мінімальний MQTT 3.1.1 publisher для shared-хостингу.
 * Без залежностей: stream_socket_client поверх TLS, CONNECT → PUBLISH(QoS 1,
 * retain) → PUBACK → DISCONNECT. Лише те, що потрібно ретранслятору.
 */
declare(strict_types=1);

function mqtt_encode_string(string $s): string
{
    return pack('n', strlen($s)) . $s;
}

function mqtt_encode_remaining_length(int $len): string
{
    $out = '';
    do {
        $byte = $len % 128;
        $len = intdiv($len, 128);
        if ($len > 0) {
            $byte |= 0x80;
        }
        $out .= chr($byte);
    } while ($len > 0);
    return $out;
}

function mqtt_build_connect(string $clientId, string $user, string $pass, int $keepalive): string
{
    $flags = 0x02; // clean session
    $payload = mqtt_encode_string($clientId);
    if ($user !== '') {
        $flags |= 0x80;
        $payload .= mqtt_encode_string($user);
        if ($pass !== '') {
            $flags |= 0x40;
            $payload .= mqtt_encode_string($pass);
        }
    }
    $varHeader = mqtt_encode_string('MQTT') . chr(0x04) . chr($flags) . pack('n', $keepalive);
    $body = $varHeader . $payload;
    return chr(0x10) . mqtt_encode_remaining_length(strlen($body)) . $body;
}

function mqtt_build_publish(string $topic, string $payload, bool $retain, int $packetId): string
{
    $fixed = 0x30 | 0x02 | ($retain ? 0x01 : 0x00); // QoS 1
    $body = mqtt_encode_string($topic) . pack('n', $packetId) . $payload;
    return chr($fixed) . mqtt_encode_remaining_length(strlen($body)) . $body;
}

/** Читає рівно $n байт або повертає null при таймауті/обриві. */
function mqtt_read_exact($sock, int $n): ?string
{
    $buf = '';
    while (strlen($buf) < $n) {
        $chunk = fread($sock, $n - strlen($buf));
        if ($chunk === false || $chunk === '') {
            return null;
        }
        $buf .= $chunk;
    }
    return $buf;
}

/** Читає один пакет: повертає [type, body] або null. */
function mqtt_read_packet($sock): ?array
{
    $hdr = mqtt_read_exact($sock, 1);
    if ($hdr === null) {
        return null;
    }
    $len = 0;
    $mult = 1;
    $more = true;
    for ($i = 0; $i < 4 && $more; $i++) {
        $b = mqtt_read_exact($sock, 1);
        if ($b === null) {
            return null;
        }
        $len += (ord($b) & 0x7f) * $mult;
        $more = (ord($b) & 0x80) !== 0;
        $mult *= 128;
    }
    if ($more) {
        return null; // Remaining Length довша за 4 байти: не MQTT
    }
    $body = $len > 0 ? mqtt_read_exact($sock, $len) : '';
    if ($body === null) {
        return null;
    }
    return [ord($hdr) >> 4, $body];
}

/**
 * $cfg: host, port (8883), user, pass, timeout (с), tls (bool, default true).
 * Повертає true, якщо брокер підтвердив PUBACK.
 */
function mqtt_publish(array $cfg, string $topic, string $payload, bool $retain = true): bool
{
    $host = (string)($cfg['host'] ?? '');
    $port = (int)($cfg['port'] ?? 8883);
    $timeout = (int)($cfg['timeout'] ?? 5);
    $tls = $cfg['tls'] ?? true;
    if ($host === '') {
        error_log('mqtt_publish: empty host');
        return false;
    }

    $ctx = stream_context_create(['ssl' => [
        'verify_peer' => true,
        'verify_peer_name' => true,
        'SNI_enabled' => true,
    ]]);
    $addr = ($tls ? 'tls://' : 'tcp://') . $host . ':' . $port;
    // TLS-рукостискання орієнтується на default_socket_timeout, а не на $timeout.
    $prevSocketTimeout = ini_get('default_socket_timeout');
    ini_set('default_socket_timeout', (string)$timeout);
    $sock = @stream_socket_client($addr, $errno, $errstr, $timeout, STREAM_CLIENT_CONNECT, $ctx);
    ini_set('default_socket_timeout', (string)$prevSocketTimeout);
    if ($sock === false) {
        error_log("mqtt_publish: connect to $addr failed: [$errno] $errstr");
        return false;
    }
    stream_set_timeout($sock, $timeout);

    try {
        $clientId = 'relay-' . bin2hex(random_bytes(4));
        $connect = mqtt_build_connect($clientId, (string)($cfg['user'] ?? ''), (string)($cfg['pass'] ?? ''), 30);
        if (fwrite($sock, $connect) !== strlen($connect)) {
            error_log('mqtt_publish: CONNECT write failed');
            return false;
        }
        $pkt = mqtt_read_packet($sock);
        if ($pkt === null || $pkt[0] !== 2 || strlen($pkt[1]) < 2) {
            error_log('mqtt_publish: no CONNACK');
            return false;
        }
        $rc = ord($pkt[1][1]);
        if ($rc !== 0) {
            error_log("mqtt_publish: CONNACK refused, code $rc");
            return false;
        }

        $publish = mqtt_build_publish($topic, $payload, $retain, 1);
        if (fwrite($sock, $publish) !== strlen($publish)) {
            error_log('mqtt_publish: PUBLISH write failed');
            return false;
        }
        $pkt = mqtt_read_packet($sock);
        if ($pkt === null || $pkt[0] !== 4 || $pkt[1] !== "\x00\x01") {
            error_log('mqtt_publish: no PUBACK');
            return false;
        }

        fwrite($sock, chr(0xE0) . chr(0x00)); // DISCONNECT
        return true;
    } finally {
        fclose($sock);
    }
}
