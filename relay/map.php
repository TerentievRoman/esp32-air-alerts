<?php
/**
 * Мапінг тіла вебхука на стан тривоги: "RED" | "YELLOW" | "CLEAR" | null.
 *
 * Формат вебхука Порталу даних Києва публічно не документований, тому тут
 * підтримуються очевидні варіанти. Коли з'явиться реальний зразок у
 * relay/log/, правити лише цей файл і relay/tests/map_tests.php.
 */
declare(strict_types=1);

const MAP_STATE_FIELDS = ['state', 'status', 'type', 'event', 'alerttype', 'action'];
const MAP_BOOL_FIELDS = ['active', 'isactive'];

const MAP_RED = ['start', 'started', 'alert', 'active', 'on', '1', 'true', 'тривога', 'початок', 'red'];
const MAP_CLEAR = ['end', 'ended', 'clear', 'inactive', 'off', '0', 'false', 'відбій', 'кінець', 'green'];
const MAP_YELLOW = ['yellow', 'warning', 'attention', 'увага'];

function map_value_to_state($value): ?string
{
    if (is_bool($value)) {
        return $value ? 'RED' : 'CLEAR';
    }
    if (!is_scalar($value)) {
        return null;
    }
    $v = mb_strtolower(trim((string)$value));
    if (in_array($v, MAP_RED, true)) {
        return 'RED';
    }
    if (in_array($v, MAP_CLEAR, true)) {
        return 'CLEAR';
    }
    if (in_array($v, MAP_YELLOW, true)) {
        return 'YELLOW';
    }
    return null;
}

function map_payload_to_state(array $payload): ?string
{
    $lower = [];
    foreach ($payload as $k => $v) {
        $lower[mb_strtolower((string)$k)] = $v;
    }
    foreach (MAP_STATE_FIELDS as $field) {
        if (array_key_exists($field, $lower)) {
            $state = map_value_to_state($lower[$field]);
            if ($state !== null) {
                return $state;
            }
        }
    }
    foreach (MAP_BOOL_FIELDS as $field) {
        if (array_key_exists($field, $lower)) {
            $v = $lower[$field];
            if (is_bool($v)) {
                return $v ? 'RED' : 'CLEAR';
            }
            return map_value_to_state($v);
        }
    }
    return null;
}
