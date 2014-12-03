--db:test
--{{{
DROP EXTENSION jsonb_extra;
CREATE EXTENSION jsonb_extra;

SELECT jsonb_extract('{"a":"1", "b":{"c":"2", "d": [1,2,{"x":100}], "e":{"f": 4}}}', '{b,e,f}');
SELECT jsonb_extract('{"b":{"c":"2"}}', '{b,c}');
SELECT jsonb_extract('{"b":[{"c":1},{"c":2},{"x":"ups"}]}', '{b,c}');


SELECT jsonb_extract('{"b":[{"c":{"x": 5}},{"c":2},{"x":"ups"}]}', '{b,c}');

--}}}
