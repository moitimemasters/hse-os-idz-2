import typing as tp


class State(tp.NamedTuple):
    repeats: int
    current_result: str


def decode_instructions(data: str) -> str:
    stack: list[State] = []
    repeats = 0
    result = ""
    for item in data:
        if item.isdigit():
            repeats = repeats * 10 + int(item)
        elif item == "[":
            stack.append((repeats, result))
            repeats = 0
            result = ""
        elif item == "]":
            saved_repeats, saved_result = stack.pop()
            result = saved_result + result * saved_repeats
    return result
