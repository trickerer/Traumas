REPLACE INTO `spell_linked_spell`
(`spell_trigger`,`spell_effect`,`type`,`comment`)
VALUES
('-455001','-455101','0','truma: eye - blindness'),
('-455002','-455102','0','truma: bone - pain'),
('-455003','-455103','0','truma: heart - bleed'),
('-455004','-455104','0','truma: brain - unconsciousness'),
('-455005','-455105','0','truma: st - fraility');

REPLACE INTO `spell_proc`
(`SpellId`,`SpellTypeMask`,`Chance`,`Cooldown`)
VALUES
('455001','1','20','4000'),
('455002','1','20','4000'),
('455003','1','33','1000'),
('455004','1','20','4000'),
('455005','1','50','250');
