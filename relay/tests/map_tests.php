<?php
// Тести map_payload_to_state; підключаються з run.php, коли є map.php.
declare(strict_types=1);
require_once __DIR__ . '/../map.php';

expect(map_payload_to_state(['status' => 'start']), 'RED', 'status start → RED');
expect(map_payload_to_state(['status' => 'END']), 'CLEAR', 'status END (регістр) → CLEAR');
expect(map_payload_to_state(['type' => 'увага']), 'YELLOW', 'type увага → YELLOW');
expect(map_payload_to_state(['active' => true]), 'RED', 'active true → RED');
expect(map_payload_to_state(['active' => false]), 'CLEAR', 'active false → CLEAR');
expect(map_payload_to_state(['foo' => 'bar']), null, 'невідоме поле → null');
expect(map_payload_to_state([]), null, 'порожній масив → null');
expect(map_payload_to_state(['State' => 'alert']), 'RED', 'ключ без урахування регістру');
expect(map_payload_to_state(['event' => 1]), 'RED', 'числове 1 → RED');
expect(map_payload_to_state(['event' => '0']), 'CLEAR', 'рядкове 0 → CLEAR');
expect(map_payload_to_state(['action' => 'тривога']), 'RED', 'тривога → RED');
expect(map_payload_to_state(['alertType' => 'відбій']), 'CLEAR', 'відбій → CLEAR');
expect(map_payload_to_state(['isActive' => true, 'status' => 'end']), 'CLEAR', 'явне поле status має пріоритет над isActive');

// Без mbstring кирилиця має працювати через фолбек.
expect(map_lower_fallback('ТРИВОГА Start'), 'тривога start', 'fallback lowercases Cyrillic and Latin');
expect(map_lower('ВІДБІЙ'), 'відбій', 'map_lower handles Cyrillic');

// Реальний формат Київ Цифрового (kyiv.digital/open-api/docs/air-alert.yaml).
$kdAlert = ['current' => ['state' => 1, 'causes' => ['drone', 'ballistic'], 'last_cause' => 'ballistic', 'created_at' => '2026-11-21 20:10:00'],
            'stats' => ['total_alerts' => 1234, 'total_lasts_for' => 123456]];
$kdClear = ['current' => ['state' => 0, 'causes' => [], 'last_cause' => null, 'created_at' => '2026-11-21 21:00:00'],
            'stats' => ['total_alerts' => 1234, 'total_lasts_for' => 123456]];
expect(map_payload_to_state($kdAlert), 'RED', 'kyiv.digital current.state=1 → RED');
expect(map_payload_to_state($kdClear), 'CLEAR', 'kyiv.digital current.state=0 → CLEAR');
expect(map_payload_to_state(['current' => ['state' => 1]]), 'RED', 'nested current without causes → RED');
expect(map_payload_causes($kdAlert), ['drone', 'ballistic'], 'causes extracted');
expect(map_payload_causes($kdClear), [], 'causes empty on clear');
expect(map_payload_causes(['status' => 'start']), [], 'causes empty when absent');
expect(map_payload_causes(['current' => ['state' => 1, 'causes' => 'drone']]), [], 'non-array causes ignored');
