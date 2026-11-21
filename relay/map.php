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

/** Нижній регістр для латиниці та кирилиці без mbstring. */
function map_lower_fallback(string $s): string
{
    static $upper = null, $lower = null;
    if ($upper === null) {
        $upper = preg_split('//u', 'АБВГҐДЕЄЖЗИІЇЙКЛМНОПРСТУФХЦЧШЩЬЮЯЁЪЫЭ', -1, PREG_SPLIT_NO_EMPTY);
        $lower = preg_split('//u', 'абвгґдеєжзиіїйклмнопрстуфхцчшщьюяёъыэ', -1, PREG_SPLIT_NO_EMPTY);
    }
    return str_replace($upper, $lower, strtolower($s));
}

function map_lower(string $s): string
{
    return function_exists('mb_strtolower') ? mb_strtolower($s, 'UTF-8') : map_lower_fallback($s);
}

function map_value_to_state($value): ?string
{
    if (is_bool($value)) {
        return $value ? 'RED' : 'CLEAR';
    }
    if (!is_scalar($value)) {
        return null;
    }
    $v = map_lower(trim((string)$value));
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

/**
 * Київ Цифровий (kyiv.digital/open-api): {"current":{"state":0|1,"causes":[...]},"stats":{...}}.
 * Повертає вкладений об'єкт current, якщо він є, інакше сам payload.
 */
function map_unwrap_current(array $payload): array
{
    foreach ($payload as $k => $v) {
        if (map_lower((string)$k) === 'current' && is_array($v)) {
            return $v;
        }
    }
    return $payload;
}

/** Список активних загроз (drone, mig, missile, ballistic, ...) або []. */
function map_payload_causes(array $payload): array
{
    $cur = map_unwrap_current($payload);
    foreach ($cur as $k => $v) {
        if (map_lower((string)$k) === 'causes') {
            return is_array($v) ? array_values(array_filter($v, 'is_string')) : [];
        }
    }
    return [];
}

/** Загрози, які Портал даних Києва відносить до жовтого рівня. */
const MAP_YELLOW_CAUSES = ['drone'];

function map_payload_to_state(array $payload): ?string
{
    $causes = map_payload_causes($payload);
    $payload = map_unwrap_current($payload);
    $lower = [];
    foreach ($payload as $k => $v) {
        $lower[map_lower((string)$k)] = $v;
    }
    foreach (MAP_STATE_FIELDS as $field) {
        if (array_key_exists($field, $lower)) {
            $state = map_value_to_state($lower[$field]);
            if ($state === 'RED' && $causes !== []
                && array_diff(array_map('map_lower', $causes), MAP_YELLOW_CAUSES) === []) {
                return 'YELLOW';
            }
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
