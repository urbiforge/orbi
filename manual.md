# Urbiscript for Unreal

## Introduction

### How it can help make better games, faster

They say that the key to success for making a successful game is to iterate, iterate, iterate, until you find the good gameplay mechanics, level design, ...

When it comes to writing gameplay code in Unreal your options are:

- C++ : hard to master, cumbersome, slow iteration cycle
- Blueprints: easy to learn, but tedious to use at scale, hard to refactor things, nearly impossible to work on collaboratively and finally less productive than real code when you start to know what you are doing.

Urbiscript is now a viable alternative: it is a dynamic programming language, originally built to control robots, with built-in support for parallelism (like golang) and advanced event management primitives. As any scripting language it is ideal to fast iterate, try ideas and prototype behaviors.

This plugin provides support for integrating urbiscript code into Unreal. You can use it to type code live while your game is running in order to test a behavior, monitor a property, inhibit some parts of the code, or even rewrite a function.

### What you can do with this plugin

- Call from within urbiscript any Unreal function on any actor, component or object.
- Read and write any property in urbiscript.
- Attach an urbiscript component to an actor that will run some code for each instance of that actor.
- Create urbiscript actor components with properties and functions visible from Unreal blueprints.
- Call urbiscript from blueprints.
- Bind delegates, legacy and enhanced input events.
- Evaluate and add code live by connecting to the urbiscript TCP port 54000 (using netcat or the GUI urbilab).

### Sample code

Let's write an actor function in urbiscript, that splits a cube into two cubes in the biggest dimension:

We'll make use of the 'transform' property which can be used to get or set the current actor position, rotation and scale.

We'll use 'safeCopy' provided by the urbiscript unreal library to clone the current actor with a given target transform.

```
	function splitBest()
	{
		var s = transform.scale.asList(); // get scale vctor
		split(s.argMax())                  // split on index with biggest size
	};
	function split(axis=0)
	{
		var t = transform;                 // get actor transform
		t.scale[axis] /= 2;
		var front = Vector.new([0,0,0]);
		front[axis] = 1;
		var fr = t.rotation.rotate(front);  // fr points in the split direction
		t.translate(fr * t.scale[0]*50);
		transform = t;                      // move this actor
		t.translate(fr * t.scale[0]*-100);
		safeCopy(t)                         // spawn a new actor at 't'
	};
```

When called it will scale the target Actor by 0.5 on one dimension, move it, and spawn a clone of itself alongside it.

## Installing the plugin

Download the plugin archive, and extract it into your project's 'Plugins' directory, creating it if needed. after that you should have one folder 'Plugins/urbi'.

## Plugin set-up

Start by copying the 'Template/urbi' directory in the plugin into your 'Content' directory.

Then add the "UrbiBridge" actor to your scene. You can configure a few things in its properties:

- ListenPort: TCP port to listen for Urbiscript code on. Useful feature to type live code. To connect, on Linux simply use `rlwrap nc localhost 54000`, on Windows download Urbilab from the Urbiscript web page. Set to -1 to disable.
- BudgetMicroseconds: Time budget allocated for Urbiscript each frame
- BudgetTick: maximum number of urbiscript instructions to run in one frame

If you will bind enhanced input actions in urbiscript, drag and drop your mapping object to the UrbiBridge 'mappingContext' property.

When you enter play mode, the Urbiscript engine will (re)start from scratch, load the provided `unreal.u` file from 'Content/urbi', then `classes.u` and `main.u`. Do not edit the first one, but the two others are free for you to put your code in.

## Packaging games with the plugin

One last step is needed in order to produce packaged games using the plugin: Open the project settings and add 'urbi/' folder to the list 'Additional non-asset directories to copy' (in the 'packaging' section). That's it.

## Attaching code to actors

You can associate an urbiscript object with each instance of an actor. To enable it:

- add the "UrbiComponent" actor component to your actor in Unreal Editor, and fill in your class name under the component's 'className' property.
- In main.u, which is loaded by urbi when you enter play mode or run the standalone game, create a class with that name and define a start() method:

```
class uobjects.Box : Unreal.UObject
{ // There will be one instance created per Actor instance
	function start()
	{ // Called at spawn time, can never return
		var this.smc = getComponent("StaticMeshComponent0");
		every(10s) // bounce around
			smc.AddForce([0,0, 500000.random()], "None", false),
	};
};
```

## Making Urbiscript actor components visible from blueprints

Note: do to the static nature of Unreal bindings the process is a bit
circunvoluted.

### Declaring classes, properties and functions in classes.u

Edit `classes.u` to put your classes that will be mapped to actor components.

You are only allowed to create classes (with Urbiscript `class` keyword) inheriting from `Unreal.UObject`, with functions and properties in them. Do *not* call any code from `classes.u`. Class name must start with 'U' that will not appear when searching for it in component class list.

To be eligible for binding, properties must have a default value that will fix their Unreal type. Allowed values and their corresponding types are:

- 0: Unreal double
- "": Unreal string
- "$0": Unreal UObject pointer
- "Foo": An instance of the struct "Foo"
- [<type>]: An array (TArray): inner array type is defined by the rules above.

To be eligible for binding, functions must either:

- Use urbiscript type checking for all arguments *and* provide a `returnType` property on the function itself specifying the return type
- Provide a `signature` property which is a list of values, one per argument and the last one for return type, using the conventions above.

Here is a valid sample to make things clearer:
```
class USampleComponent: Unreal.UObject
{
    var afloat = 0;
    var somestring = "";
    var apointer = "$0";
    var avector = "Vector";
    var avectorlist = ["Vector"];
    function sum(x:Float, y:Float) { x+y};
    var sum.returnType = 0;
    function vectorFirstComponent(v) { v[0]};
    var vectorFirstComponent.signature = ["Vector", 0];
};
class USecondSampleComponent: Unreal.UObject
{  // ...
};
```

### Parse it and generate components

Each time you make a class, property or function change, you *must*:

First hit menu "Urbi->Reload". This will parse your `classes.u` file and generate an `orbi.urbi` file.

Restart Unreal Editor.

At each launch `orbi.urbi` is parsed and the necessary boilerplate generated.

### Use it

You can then add the urbi components to any Unreal Actor, set and get properties and call functions from any blueprint.


## Additional code snippets to get you started

### Impact force when player lands a jump

The aim here is to make all boxes jump into the air when the player lands near them after a jump.
For this we need to:

- Create an 'impact' event
- Detect when the player lands on ground after a jump and emit the event
- React to the event and apply a force upwards for each box

Let's first declare the event we will use. At the top of main.u:

```
var Global.impact = Event.new();
```
That's just calling 'Event' constructor and storing the result in a globally accessible location.

Then the player jump detection goes like this:

```
//bind player to player actor
var uobjects.player = Unreal.UObject.new("BP_ThirdPersonCharacter_C_0");
detach({    // run in background
	var in_the_air = false;    // are we in the air?
	var fastest_velocity = 0;      // stores highest velocity

	at (System.tick->changed?) { // run every tick

		var z_velocity = player.GetVelocity()[2];   // Z velocity
		if (z_velocity < -100)
		{
			fastest_velocity = fastest_velocity.min(z_velocity);
			in_the_air = true;
		};

		if (z_velocity == 0 && in_the_air)
		{  // jump landed
			in_the_air = false;

			// emit impact with payload location and strength
			Global.impact!(player.transform.translation, fastest_velocity*-1);
			fastest_velocity = 0;
		};
	}
});
```

"System.tick" is a variable that gets incremented at each Unreal tick (frame). So monitoring it's "changed" event is the best way to run some code every frame.

Finally, reacting is done by listening to 'impact' in the Box object start function:


```
var impactTag = Tag.new();
function start()
{
	var this.smc = getComponent("StaticMeshComponent0");
	impactTag: at (Global.impact?(var loc, var strength))
	{ // jump when an impact is received
		var d = (transform.translation-loc).norm; // distance to impact
		strength *= 10000 / d;
		smc.AddForce([0,0,strength], "None", false); // jump
	};
};
```

And that's it.

Notice the 'impactTag' in the code above? This allows control over the execution of the code. You can tag any block of code that possibly is running in parallel in your program and orchestrate these light threads:

Typing *Box.impactTag.stop()* will stop the code from running again.

Use *Box.impactTag.freeze()* to temporarily disable it (*unfreeze* to restore).


### Raycasting and material change

Let's change the cube color when the character is looking towards it.

First we need to make a dynamic material from the mesh to be able to modify it. To do so, add in Box.start:

```
var this.mat = smc.CreateDynamicMaterialInstance(0, smc.GetMaterial(0), "None");
```

Then we'll periodically raycast using a convenient wrapper provided by this plugin, and change material color on hit:

```
var redBox = nil;
detach({
	at(System.tick->chnaged?) {
		var raycast = player.raycastForward(10000);
		if (!raycast.isNil && raycast[0].isA(Box)) //check we hit a Box
		{
			var box = raycast[0];
			box.mat.SetVectorParameterValue("Base Color", [0.9,0.2,0.2,1.0]);
			redBox = box;    // remember it
		}
		else if (!redBox.isNil)
		{
			redBox.mat.SetVectorParameterValue("Base Color", [0.2,0.2,0.9,1.0]);
			redBox = nil;
		}
	}
});
```

### Responding to user input


Actor objects have convenience functions to bind unreal user input to urbiscript events:

```
at (player.bindEnhancedInput("IA_Split", Unreal.ETriggerEvent.Started)?)
{
	var rc = player.raycastForward(10000);
	if (!rc.isNil && rc[0].isA(Box))
		rc[0].splitBest();
},
```

Note the comma at the end of the *at*, instead of a semicolon. This is a quick way to tell urbiscript to put the command in the background. Without it, if we had used a semicolon, the rest of the code would not execute because at is a blocking command. This is a common source of error in urbiscript for beginners, so beware!

To receive axis inputs, add a payload to the *at* that will be set with the 1, 2 or 3 axis value:

	function takeOver()
	{  // Make a box take over controls
		var player_controller = Unreal.bridge.getFirstPlayerController();
		player.DisableInput(player_controller);
		EnableInput(player_controller);

		var factor = 100000;
		ctrlTag: at (bindEnhancedInput("IA_Move", Unreal.ETriggerEvent.Triggered)?(var vec))
		{
			smc.AddForce([vec[0] * factor, vec[1]*factor, 0], "None", false);
		}
	};

"Unreal.ETriggerEvent" contains values for Unreal's C++ "ETriggerEvent enum".
"vec" is the variable that will be bound to the payload of the event. It can be a float or a 2 or 3 values vector depending on the axis.
