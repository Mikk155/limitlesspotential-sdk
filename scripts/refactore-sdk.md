This script has been used to refactore the repository.

You can see the commit changes in [Refactore all 'tab' indents to normal spaces](https://github.com/Mikk155/halflife-unified-sdk/commit/5a0ee1301b394eb2cdf9a7ac442b6f9464d250d5) and [Refactore all parenthesis](https://github.com/Mikk155/halflife-unified-sdk/commit/dea0406cd65848072d2b1e30fbb92ca43e13079d)

This is a simple formatting refactoring to match my liking.

For instance ``END_DATAMAP()`` will be pushed to the left while ``DEFINE_`` fields will keep all a indent.

while, for, if. they all will have their open parentheses close without a first space. the condition inside will have a space in the open and closed parentheses.

That's all.

this python script is shit but did the work for *my* liking so now i feel conforted writing.

I've upload it here just if anyone else wants to format their forks they get an idea of how i did it masively and mostly if i want to reuse this outside of the game's code (tools)
