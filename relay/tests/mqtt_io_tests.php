<?php
// Тести читання пакетів і таймаутів mqtt_publish.php (без мережі, крім localhost).
declare(strict_types=1);
require_once __DIR__ . '/../mqtt_publish.php';

function mem_stream(string $bytes)
{
    $s = fopen('php://memory', 'r+');
    fwrite($s, $bytes);
    rewind($s);
    return $s;
}

expect(mqtt_read_packet(mem_stream("\x20\x02\x00\x00")), [2, "\x00\x00"], 'read CONNACK');
expect(mqtt_read_packet(mem_stream("\x40\x02\x00\x01")), [4, "\x00\x01"], 'read PUBACK id 1');
$body128 = str_repeat('a', 128);
expect(mqtt_read_packet(mem_stream("\x30\x80\x01" . $body128)), [3, $body128], 'read 2-byte remaining length 128');
expect(mqtt_read_packet(mem_stream("\x30\x05abc")), null, 'truncated body → null');
expect(mqtt_read_packet(mem_stream("")), null, 'empty stream → null');
expect(mqtt_read_packet(mem_stream("\x30\x80\x80\x80\x80\x01")), null, 'remaining length longer than 4 bytes → null');
expect(mqtt_read_exact(mem_stream("xyz"), 2), 'xy', 'read_exact 2 of 3');
expect(mqtt_read_exact(mem_stream("x"), 2), null, 'read_exact short → null');

// TLS-рукостискання з сервером, що прийняв TCP, але мовчить: має впертися у timeout.
$srv = stream_socket_server('tcp://127.0.0.1:0', $errno, $errstr);
$port = (int)substr(stream_socket_get_name($srv, false), strrpos(stream_socket_get_name($srv, false), ':') + 1);
$t0 = microtime(true);
$ok = mqtt_publish(['host' => '127.0.0.1', 'port' => $port, 'tls' => true, 'timeout' => 2], 't', 'x');
$elapsed = microtime(true) - $t0;
expect($ok, false, 'silent TLS peer → false');
expect($elapsed < 4.0, true, "silent TLS peer respects timeout (took " . round($elapsed, 1) . "s)");
fclose($srv);
