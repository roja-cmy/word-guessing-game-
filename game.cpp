/* ============================================================================
   game.cpp — WORD HUNT : the game engine implementation
   ----------------------------------------------------------------------------
   Everything the game decides happens in this file:

     * WORD_DATABASE – 860+ words (100+ per category), each with a clue, a hint
       and an educational fact.
     * the NO-REPEAT system – a separate used-word list for every category;
       a word that has been played can never be selected again until the
       player explicitly resets that category.
     * difficulty really changes the words offered (each word carries a
       difficulty tag: EASY / MEDIUM / HARD), not only the number of lives.
     * guesses, lives, score (+10/+50/-5/-15 with the 1x/1.5x/2x multiplier),
       streak, two-stage hints, win / lose / category-completed.

   The browser version calls wordhunt_command() (WebAssembly) with a text
   command; the terminal version uses main(). Both use the same engine.
   ========================================================================== */

#include "game.h"

#include <iostream>
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <ctime>

using namespace std;

/* --------------------------------------------------------------------------
   1. GAME CONSTANTS
   -------------------------------------------------------------------------- */
const int HINTS_PER_WORD   = 2;    /* 1st = extra clue, 2nd = reveal letter  */
const int POINTS_PER_LETTER= 10;   /* per revealed letter (x multiplier)     */
const int WORD_BONUS       = 50;   /* finishing the word  (x multiplier)     */
const int WRONG_PENALTY    = 5;    /* a wrong guess                          */
const int HINT_COST        = 15;   /* each hint                              */

/* --------------------------------------------------------------------------
   2. THE WORD DATABASE  —  7 categories, 860+ entries (100+ words each)
   Fields: word, category, clue, hint1 (extra clue), hint2 (vaguer clue for
   HARD, may be empty), fact, difficulty (EASY | MEDIUM | HARD)
   -------------------------------------------------------------------------- */
WordData WORD_DATABASE[] = {

    /* =============================== ANIMALS =========================== */
    { "TIGER",       "ANIMALS",    "A large striped big cat that hunts alone in Asian jungles.",  "It is the national animal of India.",                       "", "Every tiger's stripe pattern is unique, like a fingerprint.",   "EASY"   },
    { "LION",        "ANIMALS",    "A big cat known as the king of the jungle.",                  "Only the male has a thick mane of hair.",                   "", "A lion's roar can be heard eight kilometres away.",            "EASY"   },
    { "ELEPHANT",    "ANIMALS",    "The largest land animal on Earth, famous for its trunk.",     "It has huge ears and two long tusks.",                      "", "An elephant's trunk contains about 40,000 muscles.",           "EASY"   },
    { "GIRAFFE",     "ANIMALS",    "The tallest animal in the world, with a very long neck.",     "It eats leaves from the tops of trees.",                    "", "A giraffe's tongue can be 50 centimetres long.",               "EASY"   },
    { "MONKEY",      "ANIMALS",    "A playful tree-climbing animal that loves bananas.",          "It has a long tail and clever hands.",                      "", "Monkeys make friends by grooming each other.",                 "EASY"   },
    { "KANGAROO",    "ANIMALS",    "A jumping Australian animal that carries its baby in a pouch.","It hops on two very strong legs.",                          "", "A kangaroo can jump three times its own height.",              "EASY"   },
    { "PENGUIN",     "ANIMALS",    "A black and white bird that swims but cannot fly.",           "It waddles on the ice of Antarctica.",                      "", "Emperor penguins can hold their breath for 20 minutes.",       "EASY"   },
    { "ZEBRA",       "ANIMALS",    "A wild African horse covered in black and white stripes.",    "It lives in herds on the grasslands.",                      "", "No two zebras have exactly the same stripes.",                 "EASY"   },
    { "HORSE",       "ANIMALS",    "A fast four-legged animal that people ride.",                 "It neighs and wears a saddle.",                             "", "Horses are able to sleep while standing up.",                  "EASY"   },
    { "RABBIT",      "ANIMALS",    "A small long-eared animal that hops and loves carrots.",      "It digs its home underground.",                             "", "A rabbit can rotate its ears almost all the way round.",       "EASY"   },
    { "TURTLE",      "ANIMALS",    "A slow reptile protected by a hard shell.",                   "Sea ones travel thousands of kilometres.",                  "", "Some turtles live for more than one hundred years.",           "EASY"   },
    { "FROG",        "ANIMALS",    "A small jumping amphibian that croaks near ponds.",           "It begins life as a tadpole.",                              "", "Frogs drink water through their skin.",                        "EASY"   },
    { "PEACOCK",     "ANIMALS",    "A bird famous for its huge colourful tail.",                  "The male spreads a fan of feathers.",                       "", "A peacock's tail can hold up to 200 feathers.",                "EASY"   },
    { "PARROT",      "ANIMALS",    "A colourful bird that can copy human speech.",                "It cracks nuts with a curved beak.",                        "", "Some parrots live for more than eighty years.",                "EASY"   },
    { "DOLPHIN",     "ANIMALS",    "An intelligent sea mammal that talks in clicks and whistles.","It jumps out of the water in groups.",                      "", "Dolphins sleep with one half of their brain awake.",           "MEDIUM" },
    { "LEOPARD",     "ANIMALS",    "A spotted big cat that is an excellent climber of trees.",    "It drags its food up into a tree.",                         "", "Leopards can run at almost 60 kilometres per hour.",           "MEDIUM" },
    { "SQUIRREL",    "ANIMALS",    "A small bushy-tailed animal that collects nuts.",             "It climbs trees extremely quickly.",                        "", "Squirrels plant thousands of trees by forgetting their nuts.", "MEDIUM" },
    { "OWL",         "ANIMALS",    "A night bird that can turn its head almost all the way round.","It hunts in the dark and hoots.",                           "", "An owl's eyes are fixed inside its skull.",                    "MEDIUM" },
    { "EAGLE",       "ANIMALS",    "A powerful hunting bird with very sharp eyesight.",           "It appears on many national flags.",                        "", "Eagles can spot a rabbit from three kilometres away.",         "MEDIUM" },
    { "WHALE",       "ANIMALS",    "The largest animal that has ever lived in the sea.",          "It breathes air through a blowhole.",                       "", "A blue whale's heart is the size of a small car.",             "MEDIUM" },
    { "OCTOPUS",     "ANIMALS",    "A sea creature with eight arms and three hearts.",            "It can change colour to hide.",                             "", "An octopus has blue blood.",                                  "MEDIUM" },
    { "PANDA",       "ANIMALS",    "A black and white bear that eats mostly bamboo.",             "It lives in the mountains of China.",                       "", "A panda may eat bamboo for up to fourteen hours a day.",       "MEDIUM" },
    { "KOALA",       "ANIMALS",    "An Australian animal that sleeps in eucalyptus trees.",       "It looks like a small grey bear.",                          "", "Koalas sleep for up to twenty hours a day.",                   "MEDIUM" },
    { "CHEETAH",     "ANIMALS",    "The fastest land animal on Earth.",                           "It has black tear marks on its face.",                      "", "It can reach 100 km/h in about three seconds.",                "MEDIUM" },
    { "FOX",         "ANIMALS",    "A clever bushy-tailed animal with a pointed snout.",          "It is famous in stories for being cunning.",                "", "Foxes can use the Earth's magnetic field to hunt.",            "MEDIUM" },
    { "CAMEL",       "ANIMALS",    "A desert animal with one or two humps on its back.",          "It can travel for days without water.",                     "", "A camel's hump stores fat, not water.",                        "HARD"   },
    { "SHARK",       "ANIMALS",    "An ocean predator with rows of replaceable teeth.",           "Its fin shows above the water.",                            "", "Sharks existed on Earth before trees did.",                    "HARD"   },
    { "CROCODILE",   "ANIMALS",    "A large river reptile with a long snout and a strong bite.",  "It has existed since the time of the dinosaurs.",           "", "A crocodile cannot stick out its tongue.",                     "HARD"   },
    { "WOLF",        "ANIMALS",    "A wild pack-hunting relative of the dog.",                    "It howls at night to find its group.",                      "", "A wolf howl can carry for ten kilometres.",                    "HARD"   },
    { "HIPPOPOTAMUS","ANIMALS",    "A huge African river animal with a very wide mouth.",         "Its name means river horse.",                               "", "Hippos can run faster than a human.",                          "HARD"   },
    { "RHINOCEROS",  "ANIMALS",    "A thick-skinned animal with a horn on its nose.",             "It is one of the biggest land animals.",                    "", "A rhino's horn is made of keratin, like hair.",                "HARD"   },
    { "BUTTERFLY",   "ANIMALS",    "A colourful insect that begins life as a caterpillar.",       "It has large wings and visits flowers.",                    "", "Butterflies taste their food with their feet.",                "MEDIUM" },
    { "DOG",         "ANIMALS",    "A loyal four-legged pet known as man's best friend.",         "It barks and wags its tail.",                               "", "Dogs can smell about 10,000 times better than humans.",        "EASY"   },
    { "CAT",         "ANIMALS",    "A small furry pet that purrs and chases mice.",               "It has whiskers and sharp claws.",                          "", "Cats spend about two thirds of their lives asleep.",           "EASY"   },
    { "COW",         "ANIMALS",    "A farm animal that gives us milk.",                           "It says moo and eats grass.",                               "", "Cows have best friends and get stressed when separated.",      "EASY"   },
    { "GOAT",        "ANIMALS",    "A farm animal with horns that climbs rocky hills.",           "It eats almost anything, even paper.",                      "", "Goats have rectangular pupils.",                               "EASY"   },
    { "SHEEP",       "ANIMALS",    "A woolly farm animal that lives in a flock.",                 "Its wool is used to make sweaters.",                        "", "Sheep can recognise up to fifty different faces.",             "EASY"   },
    { "DEER",        "ANIMALS",    "A gentle forest animal; the male grows antlers.",             "It is known for being fast and shy.",                       "", "Deer antlers are the fastest growing living tissue.",          "EASY"   },
    { "BEAR",        "ANIMALS",    "A large furry animal that loves honey and sleeps all winter.","It can stand on two legs.",                                 "", "Bears can run as fast as a horse over short distances.",       "EASY"   },
    { "DUCK",        "ANIMALS",    "A water bird with a flat bill that quacks.",                  "It floats on ponds and eats bread crumbs.",                 "", "A duck's quack has no echo, according to legend.",             "EASY"   },
    { "PIG",         "ANIMALS",    "A pink farm animal with a curly tail that loves mud.",        "It grunts and oinks.",                                      "", "Pigs are smarter than dogs and even some primates.",           "EASY"   },
    { "HEN",         "ANIMALS",    "A female farm bird that lays eggs.",                          "It clucks and scratches the ground.",                       "", "A hen can lay around 300 eggs in a single year.",              "EASY"   },
    { "MOUSE",       "ANIMALS",    "A tiny rodent with a long tail that loves cheese.",           "Cats love to chase it.",                                    "", "A mouse's heart beats about 600 times a minute.",              "EASY"   },
    { "BEE",         "ANIMALS",    "A striped flying insect that makes honey.",                   "It buzzes from flower to flower.",                          "", "A bee visits about 5,000 flowers in a single day.",            "EASY"   },
    { "ANT",         "ANIMALS",    "A tiny insect that lives in a colony and works hard.",        "It can carry things heavier than itself.",                  "", "Ants can lift up to fifty times their own body weight.",       "EASY"   },
    { "FISH",        "ANIMALS",    "An animal that lives in water and breathes through gills.",   "It has fins and scales.",                                   "", "Some fish can live for more than 200 years.",                  "EASY"   },
    { "CRAB",        "ANIMALS",    "A sideways-walking sea creature with two big claws.",         "It lives on beaches and rocky shores.",                     "", "Crabs can regrow a lost claw.",                                "EASY"   },
    { "SNAKE",       "ANIMALS",    "A long legless reptile that slithers along the ground.",      "Some are venomous and some squeeze their prey.",            "", "Snakes smell with their tongues.",                             "EASY"   },
    { "GORILLA",     "ANIMALS",    "The largest of all the apes, living in African forests.",     "It beats its chest and lives in family groups.",            "", "Gorillas share about 98 percent of their DNA with humans.",    "MEDIUM" },
    { "CHIMPANZEE",  "ANIMALS",    "A clever ape that uses tools and lives in Africa.",           "It is our closest living relative.",                        "", "Chimpanzees can learn to use sign language.",                  "HARD"   },
    { "ALLIGATOR",   "ANIMALS",    "A large reptile with a broad snout found in American swamps.","It looks like a crocodile but has a wider mouth.",           "", "Alligators have been around for 37 million years.",            "HARD"   },
    { "TORTOISE",    "ANIMALS",    "A slow land reptile that carries its home on its back.",      "Unlike the turtle, it lives only on land.",                 "", "Some tortoises have lived for more than 180 years.",           "MEDIUM" },
    { "SPARROW",     "ANIMALS",    "A small brown bird common in towns and gardens.",             "It chirps and hops on the ground.",                         "", "Sparrows have lived alongside humans for 10,000 years.",       "MEDIUM" },
    { "FLAMINGO",    "ANIMALS",    "A tall pink bird that stands on one leg.",                    "Its colour comes from the shrimp it eats.",                 "", "Flamingos are born grey and turn pink later.",                 "MEDIUM" },
    { "SWAN",        "ANIMALS",    "An elegant white water bird with a long curved neck.",        "It glides gracefully across lakes.",                        "", "Swans usually mate for life.",                                 "EASY"   },
    { "OSTRICH",     "ANIMALS",    "The largest bird in the world, which cannot fly.",            "It lays the biggest eggs of any bird.",                     "", "An ostrich can run at 70 kilometres per hour.",                "MEDIUM" },
    { "BUFFALO",     "ANIMALS",    "A large horned animal that pulls carts and gives milk in Asia.","It loves wallowing in muddy water.",                       "", "Water buffalo have been domesticated for 5,000 years.",        "MEDIUM" },
    { "DONKEY",      "ANIMALS",    "A hard-working animal with long ears related to the horse.",  "It brays loudly and carries heavy loads.",                  "", "Donkeys have excellent memories and recognise old friends.",   "EASY"   },
    { "PANTHER",     "ANIMALS",    "A big black cat that hunts silently at night.",               "It is really a leopard or jaguar with dark fur.",           "", "A panther's spots are still visible in bright light.",         "HARD"   },
    { "JAGUAR",      "ANIMALS",    "A powerful spotted big cat of South American jungles.",       "It has the strongest bite of all the big cats.",            "", "Jaguars love swimming and often hunt in rivers.",              "HARD"   },
    { "LLAMA",       "ANIMALS",    "A woolly South American animal used to carry loads.",         "It lives in the Andes and sometimes spits.",                "", "Llamas hum to communicate with each other.",                   "MEDIUM" },
    { "MOOSE",       "ANIMALS",    "The largest member of the deer family, with huge flat antlers.","It lives in cold northern forests.",                      "", "A moose can dive six metres underwater to eat plants.",        "HARD"   },
    { "OTTER",       "ANIMALS",    "A playful river animal that floats on its back.",             "It holds hands with its friends while sleeping.",           "", "Sea otters have the thickest fur of any animal.",              "MEDIUM" },
    { "BEAVER",      "ANIMALS",    "A big-toothed rodent that builds dams across rivers.",        "It has a flat paddle-shaped tail.",                         "", "Beaver teeth are orange because they contain iron.",           "MEDIUM" },
    { "RACCOON",     "ANIMALS",    "A masked night animal that washes its food.",                 "It has a striped bushy tail and clever paws.",              "", "Raccoons can remember solutions to puzzles for three years.",  "HARD"   },
    { "HEDGEHOG",    "ANIMALS",    "A small spiny animal that rolls into a ball when scared.",    "It comes out at night to hunt insects.",                    "", "A hedgehog has about 5,000 spines.",                           "MEDIUM" },
    { "PORCUPINE",   "ANIMALS",    "A large rodent covered in sharp quills.",                     "It raises its quills to warn enemies.",                     "", "Porcupine quills have tiny barbs that hold them in place.",    "HARD"   },
    { "SEAL",        "ANIMALS",    "A sleek sea mammal that claps its flippers.",                 "It sunbathes on rocks and beaches.",                        "", "Seals can sleep underwater and surface without waking.",        "EASY"   },
    { "WALRUS",      "ANIMALS",    "A huge Arctic sea mammal with long tusks and whiskers.",      "It uses its tusks to climb onto the ice.",                  "", "A walrus can weigh as much as a small car.",                   "MEDIUM" },
    { "JELLYFISH",   "ANIMALS",    "A soft see-through sea creature with stinging tentacles.",    "It has no brain, heart or bones.",                          "", "Some jellyfish are biologically immortal.",                    "MEDIUM" },
    { "STARFISH",    "ANIMALS",    "A five-armed sea creature that clings to rocks.",             "It can grow back a lost arm.",                              "", "Starfish have no blood; they pump sea water instead.",         "MEDIUM" },
    { "LOBSTER",     "ANIMALS",    "A large sea creature with a hard shell and big claws.",       "It turns red when cooked.",                                 "", "Lobsters taste with their legs.",                              "MEDIUM" },
    { "SEAHORSE",    "ANIMALS",    "A tiny upright fish with a head like a horse.",               "The male carries the babies.",                              "", "Seahorses are the only animals where the male gives birth.",   "HARD"   },
    { "PELICAN",     "ANIMALS",    "A big water bird with a pouch under its beak.",               "It scoops up fish with its huge bill.",                     "", "A pelican's pouch can hold three buckets of water.",           "MEDIUM" },
    { "TOUCAN",      "ANIMALS",    "A tropical bird with an enormous colourful beak.",            "It lives in South American rainforests.",                   "", "A toucan's beak is a third of its body length but very light.", "HARD"   },
    { "HUMMINGBIRD", "ANIMALS",    "A tiny bird that hovers while drinking nectar.",              "Its wings beat so fast they hum.",                          "", "It is the only bird that can fly backwards.",                  "HARD"   },
    { "WOODPECKER",  "ANIMALS",    "A bird that drums holes into tree trunks with its beak.",     "You hear it tapping in the forest.",                        "", "Its skull is built to absorb 1,000 pecks a day.",              "HARD"   },
    { "VULTURE",     "ANIMALS",    "A large bald-headed bird that circles high in the sky.",      "It cleans up by eating what other animals leave.",          "", "Vultures can spot food from four kilometres away.",            "HARD"   },
    { "PIGEON",      "ANIMALS",    "A grey city bird that coos and gathers in squares.",          "It was used to carry messages in wars.",                    "", "Pigeons can find their way home from 1,800 km away.",          "EASY"   },
    { "CROW",        "ANIMALS",    "A clever black bird with a loud caw.",                        "It can solve puzzles and remember faces.",                  "", "Crows can recognise individual human faces for years.",        "EASY"   },
    { "BAT",         "ANIMALS",    "The only mammal that can truly fly, active at night.",        "It sleeps hanging upside down.",                            "", "Bats can eat 1,000 mosquitoes in one hour.",                   "EASY"   },
    { "LIZARD",      "ANIMALS",    "A small scaly reptile that basks in the sun.",                "Some can drop their tails to escape.",                      "", "Lizards can regrow a lost tail.",                              "EASY"   },
    { "CHAMELEON",   "ANIMALS",    "A lizard famous for changing its colour.",                    "Its eyes move independently and its tongue is very long.",  "", "A chameleon's tongue can be twice its body length.",           "HARD"   },
    { "IGUANA",      "ANIMALS",    "A large green tropical lizard with a spiny back.",            "It loves basking on branches over water.",                  "", "Iguanas have a third eye on top of their heads.",              "HARD"   },
    { "GECKO",       "ANIMALS",    "A tiny lizard that can walk up walls and ceilings.",          "It chirps at night and eats insects.",                      "", "Gecko feet have millions of microscopic hairs for grip.",      "MEDIUM" },
    { "SCORPION",    "ANIMALS",    "A desert creature with pincers and a stinging tail.",         "It glows under ultraviolet light.",                         "", "Scorpions can slow their metabolism and live a year without food.", "MEDIUM" },
    { "SPIDER",      "ANIMALS",    "An eight-legged creature that spins silk webs.",              "It is not an insect.",                                      "", "Spider silk is stronger than steel of the same thickness.",    "EASY"   },
    { "MOSQUITO",    "ANIMALS",    "A tiny buzzing insect whose bite makes you itch.",            "Only the female bites.",                                    "", "Mosquitoes are attracted to people who just exercised.",       "MEDIUM" },
    { "LADYBUG",     "ANIMALS",    "A small red beetle with black spots.",                        "Gardeners love it because it eats pests.",                  "", "A ladybug can eat 5,000 insects in its lifetime.",             "EASY"   },
    { "DRAGONFLY",   "ANIMALS",    "A long-bodied insect with four shimmering wings.",            "It hovers over ponds and hunts in mid-air.",                "", "Dragonflies existed before the dinosaurs.",                    "MEDIUM" },
    { "GRASSHOPPER", "ANIMALS",    "A green jumping insect that chirps in fields.",               "It can leap twenty times its body length.",                 "", "Grasshoppers hear with organs on their bellies.",              "HARD"   },
    { "CATERPILLAR", "ANIMALS",    "The wriggly young form of a butterfly or moth.",              "It eats leaves all day before building a cocoon.",          "", "A caterpillar has about 4,000 muscles.",                       "HARD"   },
    { "SNAIL",       "ANIMALS",    "A slow slimy creature that carries a spiral shell.",          "It leaves a silvery trail behind it.",                      "", "A snail can sleep for three years.",                           "EASY"   },
    { "WORM",        "ANIMALS",    "A long soft creature that lives in the soil.",                "Birds love to eat it after rain.",                          "", "Earthworms have five hearts.",                                 "EASY"   },
    { "PONY",        "ANIMALS",    "A small horse that children love to ride.",                   "It has a thick mane and a strong body.",                    "", "Ponies are stronger for their size than full-grown horses.",   "EASY"   },
    { "BULL",        "ANIMALS",    "A strong male cow with sharp horns.",                         "It is famous for charging at a red cape.",                  "", "Bulls are actually colour-blind to red.",                      "EASY"   },
    { "YAK",         "ANIMALS",    "A long-haired ox that lives high in the Himalayas.",          "It carries loads for mountain villagers.",                  "", "Yaks can live at altitudes above 6,000 metres.",               "MEDIUM" },
    { "REINDEER",    "ANIMALS",    "An Arctic deer famous for pulling a certain sleigh.",         "Both males and females grow antlers.",                      "", "Reindeer eyes change colour from gold to blue in winter.",     "MEDIUM" },
    { "ANTELOPE",    "ANIMALS",    "A graceful fast-running animal of the African plains.",       "It has slender legs and curved horns.",                     "", "Some antelopes can jump three metres high.",                   "HARD"   },
    { "GAZELLE",     "ANIMALS",    "A slim, elegant antelope that leaps as it runs.",             "It is a favourite meal of the cheetah.",                    "", "Gazelles can run at 100 km/h in short bursts.",                "HARD"   },
    { "HYENA",       "ANIMALS",    "An African hunter famous for its laughing call.",             "It lives in clans and has powerful jaws.",                  "", "Hyenas are more closely related to cats than dogs.",           "MEDIUM" },
    { "MEERKAT",     "ANIMALS",    "A small desert animal that stands upright to keep watch.",    "It lives in big family groups in burrows.",                 "", "Meerkats are immune to some scorpion venom.",                  "MEDIUM" },
    { "LEMUR",       "ANIMALS",    "A big-eyed primate found only in Madagascar.",                "It has a long striped tail.",                               "", "Lemurs are led by the females of the group.",                  "HARD"   },
    { "SLOTH",       "ANIMALS",    "The slowest mammal, hanging upside down in trees.",           "It moves so little that algae grows on its fur.",           "", "Sloths take a month to digest a single meal.",                 "MEDIUM" },
    { "ARMADILLO",   "ANIMALS",    "A small animal wearing a suit of bony armour.",               "Some can roll into a perfect ball.",                        "", "Armadillos always give birth to identical quadruplets.",       "HARD"   },
    { "PLATYPUS",    "ANIMALS",    "A strange Australian mammal with a duck's bill that lays eggs.","It swims in rivers and has a beaver-like tail.",           "", "The platypus is one of only five egg-laying mammals.",         "HARD"   },
    { "MONGOOSE",    "ANIMALS",    "A quick small animal famous for fighting cobras.",            "It appears in a famous Jungle Book story.",                 "", "Mongooses are partly resistant to snake venom.",               "HARD"   },
    { "ORANGUTAN",   "ANIMALS",    "A red-haired ape that lives in the trees of Borneo.",         "Its name means person of the forest.",                      "", "Orangutans build a new nest to sleep in every night.",         "HARD"   },
    { "BABOON",      "ANIMALS",    "A large monkey with a dog-like face that lives in troops.",   "It has a bright red bottom.",                               "", "Baboons can live in troops of over 200 members.",              "HARD"   },
    { "PUPPY",       "ANIMALS",    "A baby dog that loves to play and chew.",                     "It is small, clumsy and very cute.",                        "", "Puppies are born deaf and blind.",                             "EASY"   },
    { "KITTEN",      "ANIMALS",    "A baby cat with soft fur and tiny claws.",                    "It loves chasing balls of wool.",                           "", "Kittens sleep up to twenty hours a day.",                      "EASY"   },
    { "ROOSTER",     "ANIMALS",    "A male farm bird that crows at sunrise.",                     "It has a red comb on its head.",                            "", "A rooster's crow can reach 130 decibels.",                     "MEDIUM" },
    { "TURKEY",      "ANIMALS",    "A large bird that gobbles and fans out its tail.",            "It is the traditional Thanksgiving dinner.",                "", "Wild turkeys can fly at 90 km/h.",                             "MEDIUM" },
    { "GOOSE",       "ANIMALS",    "A large honking water bird that flies in a V shape.",         "It hisses when it feels threatened.",                       "", "Geese can fly at heights of over 8,000 metres.",               "EASY"   },
    { "PUFFIN",      "ANIMALS",    "A black and white seabird with a colourful striped beak.",    "It is sometimes called the clown of the sea.",              "", "Puffins can carry a dozen fish in their beak at once.",        "HARD"   },
    { "ALBATROSS",   "ANIMALS",    "A giant seabird that glides for hours over the ocean.",       "It has the widest wingspan of any bird.",                   "", "An albatross can fly for years without touching land.",        "HARD"   },
    { "STINGRAY",    "ANIMALS",    "A flat sea creature that glides over the sand like a kite.",  "It hides a sharp barb in its tail.",                        "", "Stingrays are close relatives of sharks.",                     "HARD"   },
    { "SQUID",       "ANIMALS",    "A fast sea creature with ten arms that squirts ink.",         "It is a cousin of the octopus.",                            "", "The giant squid has the largest eyes in the animal kingdom.",  "MEDIUM" },
    { "CLOWNFISH",   "ANIMALS",    "A small orange and white striped reef fish.",                 "It lives safely among stinging anemones.",                  "", "Clownfish can change from male to female.",                    "MEDIUM" },
    { "SWORDFISH",   "ANIMALS",    "A large fast fish with a long pointed bill.",                 "It uses its sword to slash at prey.",                       "", "Swordfish can swim at nearly 100 km/h.",                       "HARD"   },
    { "ORCA",        "ANIMALS",    "A black and white whale also called the killer whale.",       "It is actually the largest kind of dolphin.",               "", "Orcas have their own dialects in different pods.",             "MEDIUM" },
    { "MANATEE",     "ANIMALS",    "A gentle, slow sea mammal sometimes called a sea cow.",       "It grazes on underwater grass.",                            "", "Sailors once mistook manatees for mermaids.",                  "HARD"   },
    { "NARWHAL",     "ANIMALS",    "An Arctic whale with a long spiral tusk.",                    "It is often called the unicorn of the sea.",                "", "The narwhal's tusk is really a giant tooth.",                  "HARD"   },
    { "COBRA",       "ANIMALS",    "A venomous snake that spreads a hood when angry.",            "Snake charmers are famous for playing music to it.",       "", "A king cobra can grow longer than five metres.",               "MEDIUM" },
    { "PYTHON",      "ANIMALS",    "A huge snake that squeezes its prey instead of biting.",      "It shares its name with a programming language.",           "", "A python can swallow a deer whole.",                           "MEDIUM" },
    { "TERMITE",     "ANIMALS",    "A tiny insect that eats wood and builds tall mounds.",        "It can destroy wooden houses.",                             "", "Termite mounds can be nine metres tall.",                      "HARD"   },
    { "FIREFLY",     "ANIMALS",    "A beetle whose tail glows in the dark on summer nights.",     "It flashes light to find a partner.",                       "", "A firefly's glow is almost 100 percent efficient light.",      "MEDIUM" },
    { "MOTH",        "ANIMALS",    "A night-flying cousin of the butterfly, drawn to lamps.",     "It rests with its wings flat.",                             "", "Some moths can hear the ultrasonic calls of bats.",            "EASY"   },

    /* ================================ FRUITS =========================== */
    { "MANGO",       "FRUITS",     "A sweet tropical fruit often called the king of fruits.",     "It is yellow-orange inside with one big seed.",             "", "India grows more mangoes than any other country.",             "EASY"   },
    { "APPLE",       "FRUITS",     "A crisp round fruit said to keep the doctor away.",           "It can be red, green or golden.",                           "", "There are more than 7,000 varieties of apple.",                "EASY"   },
    { "BANANA",     "FRUITS",     "A long yellow fruit that grows in bunches.",                   "Monkeys love it and it peels easily.",                      "", "A banana is botanically a berry, but a strawberry is not.",    "EASY"   },
    { "ORANGE",     "FRUITS",     "A round citrus fruit full of vitamin C.",                      "Its name is also the name of a colour.",                    "", "Oranges were first grown in China.",                           "EASY"   },
    { "WATERMELON", "FRUITS",     "A huge green fruit that is red and juicy inside.",             "It is more than ninety percent water.",                     "", "It is one of the heaviest fruits in the world.",               "EASY"   },
    { "STRAWBERRY", "FRUITS",     "A small red fruit with its seeds on the outside.",             "It is used in ice cream and jam.",                          "", "It is the only fruit that carries its seeds outside.",         "EASY"   },
    { "CHERRY",     "FRUITS",     "A small red fruit with a stone inside, often in pairs.",       "It grows on a tree in summer.",                             "", "Cherry blossoms are a national symbol of Japan.",              "EASY"   },
    { "GRAPES",     "FRUITS",     "Small round fruits that grow in bunches on vines.",            "They are used to make juice.",                              "", "Raisins are simply grapes that have been dried.",              "EASY"   },
    { "PEAR",        "FRUITS",     "A sweet bell-shaped fruit with a grainy texture.",             "It can be green, yellow or red.",                           "", "Pears ripen better after they are picked.",                    "EASY"   },
    { "PEACH",      "FRUITS",     "A soft fuzzy fruit with a rough stone in the middle.",         "Its skin feels like velvet.",                               "", "Peaches were first grown in China.",                           "EASY"   },
    { "LEMON",      "FRUITS",     "A sour yellow citrus fruit used for flavouring food.",         "It is squeezed over drinks and dishes.",                    "", "Lemons float in water, but limes sink.",                       "EASY"   },
    { "KIWI",       "FRUITS",     "A small brown fuzzy fruit with bright green flesh.",           "It is named after a New Zealand bird.",                     "", "Kiwis contain more vitamin C than oranges.",                   "EASY"   },
    { "TANGERINE",  "FRUITS",     "A small easy-to-peel citrus fruit like a flat orange.",        "It fits easily in a lunch box.",                            "", "It is named after the city of Tangier in Morocco.",            "EASY"   },
    { "BLUEBERRY",  "FRUITS",     "A tiny blue berry that grows in clusters.",                    "It is baked into muffins and pancakes.",                    "", "It is one of the few naturally blue foods.",                   "EASY"   },
    { "PAPAYA",     "FRUITS",     "A soft tropical fruit with tiny black seeds inside.",          "It is orange inside when it is ripe.",                      "", "It contains an enzyme that helps the body digest protein.",    "MEDIUM" },
    { "PINEAPPLE",  "FRUITS",     "A spiky tropical fruit wearing a crown of leaves.",            "It is sweet, yellow and used in juices.",                   "", "A single pineapple takes about two years to grow.",            "MEDIUM" },
    { "AVOCADO",    "FRUITS",     "A creamy green fruit with a large stone in the middle.",       "It is the main ingredient of guacamole.",                   "", "It contains more potassium than a banana.",                    "MEDIUM" },
    { "PLUM",       "FRUITS",     "A small smooth-skinned fruit with a flat stone inside.",       "Dried ones are called prunes.",                             "", "There are more than forty varieties of it.",                   "MEDIUM" },
    { "MELON",      "FRUITS",     "A large sweet fruit with a hard skin and soft centre.",        "It belongs to the same family as the cucumber.",            "", "Melons are almost ninety percent water.",                      "MEDIUM" },
    { "LIME",       "FRUITS",     "A small green citrus fruit with a sharp fresh taste.",         "It looks like a small lemon.",                              "", "Sailors drank its juice to avoid a disease called scurvy.",    "MEDIUM" },
    { "COCONUT",    "FRUITS",     "A large brown fruit of the palm tree with white flesh.",       "It has sweet water inside a hard shell.",                   "", "Almost every part of the coconut palm is useful.",             "MEDIUM" },
    { "GUAVA",      "FRUITS",     "A tropical fruit with pink or white flesh and many seeds.",    "It has a strong sweet smell.",                              "", "It has four times the vitamin C of an orange.",                "MEDIUM" },
    { "APRICOT",    "FRUITS",     "A small orange fruit like a peach but with smooth skin.",      "It is often eaten dried.",                                  "", "It reached Europe through Armenia.",                           "MEDIUM" },
    { "FIG",        "FRUITS",     "A soft teardrop-shaped fruit full of tiny seeds.",             "It has been eaten since ancient times.",                    "", "A fig is actually an inverted flower.",                        "MEDIUM" },
    { "BLACKBERRY", "FRUITS",     "A dark purple berry that grows on thorny bushes.",             "It is sweet and used in pies.",                             "", "It is very rich in vitamin K.",                                "MEDIUM" },
    { "RASPBERRY",  "FRUITS",     "A soft red berry made of many tiny segments.",                 "It is hollow inside when picked.",                          "", "It can be red, black or golden.",                              "MEDIUM" },
    { "LYCHEE",     "FRUITS",     "A small sweet fruit with rough red skin and white flesh.",     "It grows in bunches in Asia.",                              "", "It has been grown in China for two thousand years.",           "HARD"   },
    { "MULBERRY",   "FRUITS",     "A berry that grows on trees and stains your fingers.",         "Silkworms eat the leaves of its tree.",                     "", "Silk comes from silkworms that feed on these leaves.",          "HARD"   },
    { "POMEGRANATE","FRUITS",     "A round fruit packed with hundreds of juicy seeds.",           "It has a thick red skin.",                                  "", "A single one can contain over a thousand seeds.",              "HARD"   },
    { "CANTALOUPE", "FRUITS",     "A sweet orange-fleshed melon with netted skin.",               "It is served in fruit salads.",                             "", "It is also known as the muskmelon.",                           "HARD"   },
    { "PASSIONFRUIT","FRUITS",    "A small purple fruit with jelly-like seeds inside.",           "It has a very strong aroma.",                               "", "Its vine grows one of the prettiest flowers on Earth.",        "HARD"   },
    { "STARFRUIT",  "FRUITS",     "A yellow fruit that forms a star shape when sliced.",          "It originally comes from Southeast Asia.",                  "", "It is also called carambola.",                                 "HARD"   },
    { "DATE",       "FRUITS",     "A sweet sticky brown fruit that grows on desert palms.",       "It is eaten at sunset during Ramadan.",                     "", "Date palms can produce fruit for more than 100 years.",        "EASY"   },
    { "DRAGONFRUIT","FRUITS",     "A bright pink fruit with green scales and speckled white flesh.","It grows on a climbing cactus.",                           "", "Its flowers bloom only at night.",                             "HARD"   },
    { "JACKFRUIT",  "FRUITS",     "The largest fruit that grows on a tree, with a bumpy green skin.","Its yellow pods are sweet and its flesh is used like meat.","", "A single jackfruit can weigh more than 40 kilograms.",         "HARD"   },
    { "MANDARIN",   "FRUITS",     "A small sweet loose-skinned orange.",                          "It is named after officials of imperial China.",            "", "Mandarins are the ancestors of most other oranges.",           "MEDIUM" },
    { "CRANBERRY",  "FRUITS",     "A small sour red berry that grows in bogs.",                   "It is served as a sauce with turkey.",                      "", "Ripe cranberries bounce like a rubber ball.",                  "MEDIUM" },
    { "GOOSEBERRY", "FRUITS",     "A small round tart berry with faint stripes.",                 "It is green, yellow or red and used in pies.",              "", "Gooseberry bushes are covered in sharp thorns.",               "HARD"   },
    { "RAISIN",     "FRUITS",     "A small wrinkled dried grape.",                                "It is added to cakes, cereals and puddings.",               "", "It takes about four kilograms of grapes to make one of raisins.", "EASY"  },
    { "NECTARINE",  "FRUITS",     "A smooth-skinned cousin of the peach.",                        "It looks like a peach without the fuzz.",                   "", "A nectarine is simply a peach with a recessive gene.",         "HARD"   },
    { "PERSIMMON",  "FRUITS",     "An orange tomato-shaped fruit that is honey-sweet when ripe.", "Unripe ones make your mouth feel dry.",                     "", "Persimmon means food of the gods in Greek.",                   "HARD"   },
    { "KUMQUAT",    "FRUITS",     "A tiny orange citrus fruit eaten whole with its skin.",        "The peel is sweet and the inside is sour.",                 "", "Kumquats are the only citrus fruit eaten skin and all.",       "HARD"   },
    { "CLEMENTINE", "FRUITS",     "A small seedless sweet orange that peels easily.",             "It is a favourite in winter fruit bowls.",                  "", "It was named after a French monk called Clement.",             "MEDIUM" },
    { "GRAPEFRUIT", "FRUITS",     "A large sour citrus fruit with pink or yellow flesh.",         "It is often eaten at breakfast with a spoon.",              "", "It got its name because it grows in grape-like clusters.",     "MEDIUM" },
    { "POMELO",     "FRUITS",     "The largest citrus fruit, with a very thick pale rind.",       "It tastes like a mild sweet grapefruit.",                   "", "A pomelo can grow as big as a football.",                      "HARD"   },
    { "QUINCE",     "FRUITS",     "A hard yellow fruit that must be cooked before eating.",       "It is made into jelly and paste.",                          "", "The word marmalade originally meant quince jam.",              "HARD"   },
    { "TAMARIND",   "FRUITS",     "A brown pod with sticky sweet-and-sour pulp.",                 "It flavours chutneys, candies and sauces.",                 "", "Tamarind is a key ingredient in Worcestershire sauce.",        "HARD"   },
    { "DURIAN",     "FRUITS",     "A spiky fruit famous for its very strong smell.",              "It is banned on many trains and in hotels in Asia.",        "", "Durian is called the king of fruits in Southeast Asia.",       "HARD"   },
    { "SOURSOP",    "FRUITS",     "A large green spiky fruit with creamy white flesh.",           "Its flavour is like strawberry and pineapple together.",    "", "Soursop is also called graviola.",                             "HARD"   },
    { "CUSTARDAPPLE","FRUITS",    "A bumpy green fruit with sweet, creamy, custard-like flesh.",  "It is also known as sitaphal.",                             "", "Its pulp is often turned into ice cream.",                     "HARD"   },
    { "ELDERBERRY", "FRUITS",     "A tiny dark purple berry that grows in flat clusters.",        "It is made into syrup and wine.",                           "", "Elderberries must be cooked before they are eaten.",           "HARD"   },
    { "BOYSENBERRY","FRUITS",     "A large dark berry that is a cross of raspberry and blackberry.","It was first grown on a farm in California.",             "", "It is named after the farmer Rudolph Boysen.",                 "HARD"   },
    { "HONEYDEW",   "FRUITS",     "A smooth pale green melon with sweet juicy flesh.",            "It is served in fruit salads and smoothies.",               "", "Honeydew melons ripen only on the vine, not after picking.",   "MEDIUM" },
    { "SAPOTA",     "FRUITS",     "A brown fruit that tastes like sweet caramel.",                "It is also called chikoo.",                                 "", "Its sap was once used to make chewing gum.",                   "HARD"   },
    { "JAMUN",      "FRUITS",     "A deep purple Indian summer fruit that stains the tongue.",    "It is also called the Java plum.",                          "", "Jamun trees can live for more than 100 years.",                "HARD"   },
    { "RAMBUTAN",   "FRUITS",     "A red hairy tropical fruit with a sweet white centre.",        "Its name means hairy in Malay.",                            "", "Rambutan is a close cousin of the lychee.",                    "HARD"   },
    { "MANGOSTEEN", "FRUITS",     "A purple fruit with sweet white segments inside.",             "It is called the queen of fruits.",                         "", "Queen Victoria reportedly offered a reward for a fresh one.",  "HARD"   },
    { "LONGAN",     "FRUITS",     "A small round brown fruit with translucent sweet flesh.",      "Its name means dragon's eye.",                              "", "Longan is often dried and used in soups and desserts.",        "HARD"   },
    { "ACAI",       "FRUITS",     "A small dark purple Amazon berry famous in smoothie bowls.",   "It grows on tall palm trees in Brazil.",                    "", "Acai berries spoil within a day of being picked.",             "HARD"   },
    { "OLIVE",      "FRUITS",     "A small green or black fruit pressed to make oil.",            "It grows on ancient Mediterranean trees.",                  "", "Some olive trees are over 2,000 years old.",                   "MEDIUM" },
    { "TOMATO",     "FRUITS",     "A red juicy fruit that most people treat as a vegetable.",     "It is the base of ketchup and pizza sauce.",                "", "There are more than 10,000 varieties of tomato.",              "EASY"   },
    { "BERRY",      "FRUITS",     "Any small juicy fruit that grows on a bush.",                  "Straw, blue and black are kinds of it.",                    "", "Bananas and grapes are technically berries too.",              "EASY"   },
    { "CURRANT",    "FRUITS",     "A tiny tart berry that comes in red, black or white.",         "It is made into jams and cordials.",                        "", "Blackcurrants have four times the vitamin C of oranges.",      "HARD"   },
    { "PLANTAIN",   "FRUITS",     "A large starchy cousin of the banana that is cooked before eating.","It is fried into chips in many countries.",            "", "Plantains are a staple food for millions of people.",          "HARD"   },
    { "BREADFRUIT", "FRUITS",     "A large green fruit that tastes like bread when baked.",       "It grows on tall trees in the Pacific islands.",            "", "One tree can produce 200 fruits a year.",                      "HARD"   },
    { "CHERIMOYA",  "FRUITS",     "A green heart-shaped fruit with creamy sweet flesh.",          "Mark Twain called it the most delicious fruit known.",      "", "Cherimoya seeds are poisonous and must not be eaten.",         "HARD"   },
    { "LOQUAT",     "FRUITS",     "A small yellow-orange fruit that ripens in spring.",           "It has large shiny brown seeds.",                           "", "Loquat trees blossom in autumn, unlike most fruit trees.",     "HARD"   },
    { "SATSUMA",    "FRUITS",     "A seedless easy-peel orange from Japan.",                      "It is a type of mandarin.",                                 "", "Satsumas were first brought to the West in the 1870s.",        "HARD"   },
    { "AMLA",       "FRUITS",     "A small sour green Indian gooseberry used in pickles.",        "It is famous in Ayurvedic medicine.",                       "", "Amla has one of the highest vitamin C contents of any fruit.", "HARD"   },
    { "BAEL",       "FRUITS",     "A hard-shelled Indian fruit made into a cooling summer drink.","It is also called wood apple.",                             "", "The bael tree is considered sacred in India.",                 "HARD"   },
    { "PRUNE",      "FRUITS",     "A dried plum that is dark and wrinkly.",                       "It is well known for helping digestion.",                   "", "Prunes are made from special plum varieties with firm flesh.", "MEDIUM" },
    { "CITRON",     "FRUITS",     "A large bumpy yellow citrus fruit with a very thick peel.",    "It is one of the three original citrus fruits.",            "", "Citron peel is candied and used in fruit cakes.",              "HARD"   },
    { "BERGAMOT",   "FRUITS",     "A fragrant citrus fruit whose oil flavours Earl Grey tea.",    "It grows mainly in southern Italy.",                        "", "Bergamot oil is used in many perfumes.",                       "HARD"   },
    { "SALAK",      "FRUITS",     "A brown fruit with scaly skin, also called snake fruit.",      "It grows on a palm in Indonesia.",                          "", "Its skin looks exactly like snake scales.",                    "HARD"   },
    { "MEDLAR",     "FRUITS",     "An old-fashioned fruit eaten only when it has gone soft.",     "It looks like a small brown apple.",                        "", "Medlars were popular in medieval Europe.",                     "HARD"   },
    { "MIRACLEFRUIT","FRUITS",    "A small red berry that makes sour foods taste sweet.",         "After eating it, lemons taste like candy.",                 "", "Its effect lasts for about an hour.",                          "HARD"   },
    { "UGLIFRUIT",  "FRUITS",     "A wrinkly Jamaican citrus fruit that tastes sweeter than it looks.","It is a cross between a grapefruit and a tangerine.",  "", "Its odd name is a registered trademark.",                      "HARD"   },
    { "YUZU",       "FRUITS",     "A small fragrant Japanese citrus fruit.",                      "Its juice and zest flavour sauces and desserts.",           "", "People in Japan bathe with yuzu on the winter solstice.",      "HARD"   },
    { "CHICKOO",    "FRUITS",     "A round brown fruit with grainy, very sweet flesh.",           "It is another name for sapodilla.",                         "", "Chickoo trees can bear fruit twice a year.",                   "HARD"   },
    { "WOODAPPLE",  "FRUITS",     "A hard-shelled brown fruit with a tangy pulp.",                "You need a hammer to open it.",                             "", "Its pulp is eaten with sugar or made into jam.",               "HARD"   },
    { "PUMPKIN",    "FRUITS",     "A big round orange fruit carved at Halloween.",                "It is baked into pies and soups.",                          "", "The heaviest pumpkin ever grown weighed over a tonne.",        "EASY"   },
    { "CUCUMBER",   "FRUITS",     "A long green crunchy fruit eaten in salads.",                  "It is mostly water and very cooling.",                      "", "Cucumbers belong to the same family as melons.",               "EASY"   },
    { "ROSEHIP",    "FRUITS",     "The small red fruit of the rose plant.",                       "It is made into tea and syrup.",                            "", "Rosehips were used for vitamin C during World War II.",        "HARD"   },
    { "BILBERRY",   "FRUITS",     "A small wild European cousin of the blueberry.",               "It stains everything purple.",                              "", "Pilots once ate bilberries hoping to improve night vision.",   "HARD"   },
    { "CLOUDBERRY", "FRUITS",     "A rare amber berry that grows in cold northern bogs.",         "It is a delicacy in Scandinavia.",                          "", "Cloudberries are sometimes called Arctic gold.",               "HARD"   },
    { "LINGONBERRY","FRUITS",     "A small tart red berry served with Swedish meatballs.",        "It grows wild in Nordic forests.",                          "", "Lingonberries stay fresh for months without refrigeration.",   "HARD"   },
    { "HUCKLEBERRY","FRUITS",     "A small dark wild berry beloved in the American Northwest.",   "It shares its name with a famous Mark Twain character.",    "", "Huckleberries have never been successfully farmed.",           "HARD"   },
    { "MARIONBERRY","FRUITS",     "A large sweet blackberry variety from Oregon.",                "It is called the cabernet of blackberries.",                "", "Oregon grows almost all of the world's marionberries.",        "HARD"   },
    { "FEIJOA",     "FRUITS",     "A green egg-shaped fruit with a sweet perfumed flavour.",      "It is also called pineapple guava.",                        "", "Feijoas are hugely popular in New Zealand.",                   "HARD"   },
    { "SUGARCANE",  "FRUITS",     "A tall sweet grass whose juice is pressed into a refreshing drink.","Chewing the raw stick is a popular street snack.",       "", "Most of the world's sugar comes from it.",                     "MEDIUM" },
    { "COCONUTWATER","FRUITS",    "The clear sweet liquid found inside a young green coconut.",   "It is sold from carts on tropical beaches.",                "", "It is naturally rich in potassium.",                           "HARD"   },

    /* ============================== COUNTRIES ========================== */
    { "INDIA",       "COUNTRIES",  "The world's largest democracy and home of the Taj Mahal.",    "Its capital is New Delhi.",                                 "", "India recognises 22 official languages.",                      "EASY"   },
    { "FRANCE",      "COUNTRIES",  "A European country famous for the Eiffel Tower.",              "Its capital city is Paris.",                                "", "It is the most visited country in the world.",                 "EASY"   },
    { "CANADA",      "COUNTRIES",  "The second largest country, famous for maple syrup.",          "Its flag shows a red maple leaf.",                          "", "It holds more lakes than all other countries combined.",       "EASY"   },
    { "ITALY",       "COUNTRIES",  "A boot-shaped country famous for pizza and Rome.",             "Its capital has a famous Colosseum.",                       "", "It has more UNESCO World Heritage sites than any other.",      "EASY"   },
    { "SPAIN",       "COUNTRIES",  "A sunny European country famous for flamenco and paella.",     "Madrid is its capital.",                                    "", "Spanish is the second most spoken language on Earth.",         "EASY"   },
    { "IRELAND",     "COUNTRIES",  "A green island nation famous for Saint Patrick.",              "Its national symbol is a shamrock.",                        "", "There are no wild snakes in Ireland.",                         "EASY"   },
    { "JAPAN",       "COUNTRIES",  "An island nation famous for samurai, sushi and technology.",   "It is called the Land of the Rising Sun.",                  "", "It is made up of more than 6,800 islands.",                    "MEDIUM" },
    { "BRAZIL",      "COUNTRIES",  "The largest country in South America.",                        "It hosts a famous carnival in Rio.",                        "", "It has won the FIFA World Cup five times.",                    "MEDIUM" },
    { "GERMANY",     "COUNTRIES",  "A European country famous for cars, castles and engineering.", "Berlin is its capital city.",                               "", "It has more than twenty thousand castles.",                    "MEDIUM" },
    { "AUSTRALIA",   "COUNTRIES",  "A country that is also a whole continent.",                    "Kangaroos live there and Canberra is the capital.",        "", "It has more kangaroos than people.",                           "MEDIUM" },
    { "CHINA",       "COUNTRIES",  "The world's most populous country, home of the Great Wall.",   "Its capital is Beijing.",                                   "", "The Great Wall is over 21,000 kilometres long.",               "MEDIUM" },
    { "MEXICO",      "COUNTRIES",  "A North American country famous for tacos and ancient ruins.", "Its capital is one of the largest cities on Earth.",        "", "Chocolate was invented in this region.",                       "MEDIUM" },
    { "GREECE",      "COUNTRIES",  "A Mediterranean country known for islands and ancient gods.",  "The Olympic Games began there.",                            "", "It has around six thousand islands.",                          "MEDIUM" },
    { "SWEDEN",      "COUNTRIES",  "A Nordic country known for forests, lakes and design.",        "Its capital is Stockholm.",                                 "", "It recycles so well that it imports rubbish.",                "MEDIUM" },
    { "PORTUGAL",    "COUNTRIES",  "A coastal European country that led the age of exploration.",  "Its explorers reached India by sea.",                       "", "It is the oldest nation-state in Europe.",                     "MEDIUM" },
    { "ENGLAND",     "COUNTRIES",  "A country within the United Kingdom, the home of football.",   "London is its capital.",                                    "", "The world's first public railway ran there.",                  "MEDIUM" },
    { "TURKEY",      "COUNTRIES",  "A country that sits in both Europe and Asia.",                 "Its largest city spans two continents.",                    "", "Saint Nicholas was born in this country.",                     "MEDIUM" },
    { "VIETNAM",     "COUNTRIES",  "A long thin Southeast Asian country famous for rice.",         "Its capital is Hanoi.",                                     "", "It is the world's second largest coffee exporter.",            "MEDIUM" },
    { "NEPAL",       "COUNTRIES",  "A Himalayan country that includes the world's highest peak.",  "Kathmandu is its capital.",                                 "", "Its flag is the only non-rectangular national flag.",          "MEDIUM" },
    { "FINLAND",     "COUNTRIES",  "A Nordic country of lakes, saunas and northern lights.",       "It is called the land of a thousand lakes.",                "", "It has around 188,000 lakes.",                                 "MEDIUM" },
    { "SCOTLAND",    "COUNTRIES",  "A country famous for kilts, bagpipes and highlands.",          "Its famous monster lives in a loch.",                       "", "The game of golf began in this country.",                      "MEDIUM" },
    { "ICELAND",     "COUNTRIES",  "A volcanic island country with geysers and glaciers.",         "Its capital is Reykjavik.",                                 "", "It runs almost entirely on renewable energy.",                 "MEDIUM" },
    { "EGYPT",       "COUNTRIES",  "An African country famous for pyramids and a very long river.","Its ancient kings were called pharaohs.",                  "", "The Great Pyramid was the tallest building for 3,800 years.",  "HARD"   },
    { "NORWAY",      "COUNTRIES",  "A Scandinavian country of fjords and the northern lights.",    "It is famous for its very long coastline.",                "", "Its coastline would circle the Earth twice.",                  "HARD"   },
    { "KENYA",       "COUNTRIES",  "An East African country famous for safaris and long-distance runners.","The equator passes through it.",                    "", "It is home to the Great Rift Valley.",                         "HARD"   },
    { "THAILAND",    "COUNTRIES",  "A Southeast Asian country famous for beaches and temples.",    "Its food is known for pad thai.",                           "", "It is the only Southeast Asian country never colonised.",      "HARD"   },
    { "RUSSIA",      "COUNTRIES",  "The largest country in the world by area.",                    "It spans eleven time zones.",                               "", "It covers about one eighth of the Earth's land.",              "HARD"   },
    { "MOROCCO",     "COUNTRIES",  "A North African country of souks, deserts and mint tea.",      "The city of Marrakesh is there.",                           "", "Part of the Sahara desert lies inside it.",                    "HARD"   },
    { "PERU",        "COUNTRIES",  "A South American country home to a famous Inca city.",         "Machu Picchu is located here.",                            "", "It grows more than three thousand kinds of potato.",           "HARD"   },
    { "CHILE",       "COUNTRIES",  "A very long thin country along South America's west coast.",   "The driest desert on Earth is there.",                      "", "The country is over 4,300 kilometres long.",                   "HARD"   },
    { "SWITZERLAND", "COUNTRIES",  "A mountainous country famous for watches, chocolate and banks.","The Alps cover most of it.",                              "", "It has four national languages.",                              "HARD"   },
    { "SINGAPORE",   "COUNTRIES",  "A tiny island city-state that is a major financial hub.",      "It is famous for being extremely clean.",                  "", "It has one of the busiest ports in the world.",                "HARD"   },
    { "PAKISTAN",    "COUNTRIES",  "A South Asian country whose capital is Islamabad.",            "It is famous for cricket and the K2 mountain.",             "", "It is home to K2, the second highest mountain on Earth.",      "EASY"   },
    { "BANGLADESH",  "COUNTRIES",  "A river-rich South Asian country with the capital Dhaka.",     "It is one of the world's largest exporters of clothes.",    "", "It has the world's largest river delta.",                      "MEDIUM" },
    { "SRILANKA",    "COUNTRIES",  "A teardrop-shaped island nation south of India.",              "It is famous for tea and cricket.",                         "", "It was once called Ceylon.",                                   "MEDIUM" },
    { "BHUTAN",      "COUNTRIES",  "A small Himalayan kingdom that measures happiness.",           "It is called the Land of the Thunder Dragon.",              "", "It is the only carbon-negative country in the world.",         "HARD"   },
    { "MALAYSIA",    "COUNTRIES",  "A Southeast Asian country famous for its twin towers.",        "Its capital is Kuala Lumpur.",                              "", "Its Petronas Towers were once the tallest buildings on Earth.", "MEDIUM" },
    { "INDONESIA",   "COUNTRIES",  "The largest island country in the world.",                     "Bali and Jakarta are found here.",                          "", "It is made up of more than 17,000 islands.",                   "MEDIUM" },
    { "PHILIPPINES", "COUNTRIES",  "A Southeast Asian nation of more than 7,000 islands.",         "Its capital is Manila.",                                    "", "It is the world's largest exporter of coconuts.",              "HARD"   },
    { "KOREA",       "COUNTRIES",  "An East Asian country famous for K-pop and kimchi.",           "Its capital is Seoul.",                                     "", "South Korea has the fastest average internet speed on Earth.", "MEDIUM" },
    { "MONGOLIA",    "COUNTRIES",  "A vast landlocked country of grassy steppes and nomads.",      "Genghis Khan came from here.",                              "", "It is the least densely populated country in the world.",      "HARD"   },
    { "IRAN",        "COUNTRIES",  "A Middle Eastern country once known as Persia.",               "Its capital is Tehran.",                                    "", "It has one of the oldest civilisations in the world.",         "MEDIUM" },
    { "IRAQ",        "COUNTRIES",  "A Middle Eastern country home to ancient Mesopotamia.",        "Its capital is Baghdad.",                                   "", "Writing was invented in this region 5,000 years ago.",         "HARD"   },
    { "ISRAEL",      "COUNTRIES",  "A small Middle Eastern country on the Mediterranean coast.",   "Jerusalem is its most famous city.",                        "", "The Dead Sea here is the lowest point on land.",               "MEDIUM" },
    { "JORDAN",      "COUNTRIES",  "A Middle Eastern kingdom home to the rose-red city of Petra.", "Its capital is Amman.",                                     "", "Petra was carved directly into pink sandstone cliffs.",        "HARD"   },
    { "QATAR",       "COUNTRIES",  "A small wealthy Gulf state that hosted the 2022 World Cup.",   "Its capital is Doha.",                                      "", "It is one of the richest countries per person on Earth.",      "HARD"   },
    { "OMAN",        "COUNTRIES",  "A peaceful sultanate on the Arabian Peninsula.",               "Its capital is Muscat.",                                    "", "Oman is famous for its frankincense trees.",                   "HARD"   },
    { "KUWAIT",      "COUNTRIES",  "A small oil-rich Gulf country.",                               "Its capital shares the country's name.",                    "", "Kuwait has no natural rivers or lakes.",                       "HARD"   },
    { "AFGHANISTAN", "COUNTRIES",  "A mountainous landlocked country in Central Asia.",            "Its capital is Kabul.",                                     "", "It was a key stop on the ancient Silk Road.",                  "HARD"   },
    { "KAZAKHSTAN",  "COUNTRIES",  "The largest landlocked country in the world.",                 "Rockets launch to space from here.",                        "", "Apples originally came from its mountains.",                   "HARD"   },
    { "UZBEKISTAN",  "COUNTRIES",  "A Central Asian country famous for the Silk Road city of Samarkand.","Its capital is Tashkent.",                            "", "It is one of only two doubly landlocked countries.",           "HARD"   },
    { "MYANMAR",     "COUNTRIES",  "A Southeast Asian country of golden pagodas, once called Burma.","Its largest city is Yangon.",                             "", "It has thousands of ancient temples at Bagan.",                "HARD"   },
    { "CAMBODIA",    "COUNTRIES",  "A Southeast Asian country home to the temple of Angkor Wat.",  "Its capital is Phnom Penh.",                                "", "Angkor Wat is the largest religious monument in the world.",   "HARD"   },
    { "LAOS",        "COUNTRIES",  "A landlocked Southeast Asian country along the Mekong River.", "Its capital is Vientiane.",                                 "", "It is the most heavily bombed country in history per person.", "HARD"   },
    { "TAIWAN",      "COUNTRIES",  "An island famous for making the world's computer chips.",      "Its capital is Taipei.",                                    "", "It produces most of the world's advanced microchips.",         "HARD"   },
    { "MALDIVES",    "COUNTRIES",  "A nation of tiny coral islands famous for honeymoons.",        "It is the lowest lying country on Earth.",                  "", "Its highest natural point is only 2.4 metres above sea level.", "MEDIUM" },
    { "AUSTRIA",     "COUNTRIES",  "An Alpine country famous for Mozart and classical music.",     "Its capital is Vienna.",                                    "", "The Sound of Music was set here.",                             "MEDIUM" },
    { "BELGIUM",     "COUNTRIES",  "A small European country famous for chocolate and waffles.",   "Its capital Brussels hosts the European Union.",            "", "Belgium invented french fries, not France.",                   "MEDIUM" },
    { "NETHERLANDS", "COUNTRIES",  "A flat European country of tulips, windmills and bicycles.",   "Its capital is Amsterdam.",                                 "", "About a quarter of the country lies below sea level.",         "MEDIUM" },
    { "DENMARK",     "COUNTRIES",  "A Scandinavian country famous for LEGO and Vikings.",          "Its capital is Copenhagen.",                                "", "Denmark is often ranked the happiest country in the world.",   "MEDIUM" },
    { "POLAND",      "COUNTRIES",  "A Central European country whose capital is Warsaw.",          "Marie Curie and Chopin were born here.",                    "", "Poland has one of the oldest salt mines still open to visit.", "MEDIUM" },
    { "HUNGARY",     "COUNTRIES",  "A Central European country famous for goulash and thermal baths.","Its capital is Budapest.",                               "", "The Rubik's Cube was invented here.",                          "MEDIUM" },
    { "CROATIA",     "COUNTRIES",  "A Mediterranean country with a thousand islands.",             "Game of Thrones was filmed in Dubrovnik.",                  "", "The necktie was invented by Croatian soldiers.",               "HARD"   },
    { "UKRAINE",     "COUNTRIES",  "The largest country entirely within Europe.",                  "Its capital is Kyiv.",                                      "", "Its flag shows a blue sky over golden wheat fields.",          "MEDIUM" },
    { "ROMANIA",     "COUNTRIES",  "A Balkan country of castles and the legend of Dracula.",       "Its capital is Bucharest.",                                 "", "It has the heaviest building in the world, its parliament.",   "HARD"   },
    { "BULGARIA",    "COUNTRIES",  "A Balkan country famous for roses and yoghurt.",               "Its capital is Sofia.",                                     "", "Bulgaria produces most of the world's rose oil.",              "HARD"   },
    { "SERBIA",      "COUNTRIES",  "A landlocked Balkan country whose capital is Belgrade.",       "Tennis star Novak Djokovic comes from here.",               "", "Belgrade is one of the oldest cities in Europe.",              "HARD"   },
    { "CZECHIA",     "COUNTRIES",  "A Central European country famous for Prague and beer.",       "It was once part of Czechoslovakia.",                       "", "It drinks more beer per person than any other country.",       "HARD"   },
    { "SLOVAKIA",    "COUNTRIES",  "A small mountainous country with more than 100 castles.",      "Its capital is Bratislava.",                                "", "It has the highest number of castles per person in the world.", "HARD"  },
    { "SLOVENIA",    "COUNTRIES",  "A green Alpine country with a famous lake island at Bled.",    "Its capital is Ljubljana.",                                 "", "More than half the country is covered in forest.",             "HARD"   },
    { "ESTONIA",     "COUNTRIES",  "A Baltic country that runs almost everything online.",         "Its capital is Tallinn.",                                   "", "Skype was invented here.",                                     "HARD"   },
    { "LATVIA",      "COUNTRIES",  "A Baltic country famous for its art nouveau capital, Riga.",   "It lies between Estonia and Lithuania.",                    "", "Riga has the largest collection of art nouveau buildings.",    "HARD"   },
    { "LITHUANIA",   "COUNTRIES",  "The southernmost of the three Baltic states.",                 "Its capital is Vilnius.",                                   "", "Basketball is treated almost like a religion here.",           "HARD"   },
    { "LUXEMBOURG",  "COUNTRIES",  "A tiny but very wealthy country between France and Germany.",  "All public transport here is free.",                        "", "It has the highest GDP per person in the world.",              "HARD"   },
    { "MONACO",      "COUNTRIES",  "A tiny glamorous country on the French Riviera.",              "It hosts a famous Formula One race through its streets.",   "", "It is the second smallest country in the world.",              "HARD"   },
    { "MALTA",       "COUNTRIES",  "A small sunny island nation in the middle of the Mediterranean.","Its capital is Valletta.",                                "", "Malta has temples older than the pyramids.",                   "HARD"   },
    { "CYPRUS",      "COUNTRIES",  "An island country in the eastern Mediterranean.",              "Legend says the goddess Aphrodite was born here.",          "", "It has the oldest wine label in the world, Commandaria.",      "HARD"   },
    { "ALBANIA",     "COUNTRIES",  "A Balkan country on the Adriatic coast.",                      "Its capital is Tirana.",                                    "", "Mother Teresa was of Albanian heritage.",                      "HARD"   },
    { "WALES",       "COUNTRIES",  "A country in the United Kingdom famous for dragons and rugby.","Its capital is Cardiff.",                                   "", "It has more castles per square mile than any other country.",  "MEDIUM" },
    { "NIGERIA",     "COUNTRIES",  "The most populous country in Africa.",                         "Its capital is Abuja and its film industry is Nollywood.",  "", "Nollywood makes more films per year than Hollywood.",          "MEDIUM" },
    { "GHANA",       "COUNTRIES",  "A West African country once known as the Gold Coast.",        "Its capital is Accra.",                                     "", "It was the first African colony to gain independence.",        "HARD"   },
    { "ETHIOPIA",    "COUNTRIES",  "An East African country where coffee was first discovered.",   "Its capital is Addis Ababa.",                               "", "It follows a calendar that is seven years behind ours.",       "HARD"   },
    { "TANZANIA",    "COUNTRIES",  "An East African country home to Kilimanjaro and Zanzibar.",    "The Serengeti migration happens here.",                     "", "Kilimanjaro is Africa's highest mountain.",                    "HARD"   },
    { "UGANDA",      "COUNTRIES",  "An East African country called the Pearl of Africa.",          "Its capital is Kampala.",                                   "", "Half of the world's mountain gorillas live here.",             "HARD"   },
    { "ZIMBABWE",    "COUNTRIES",  "A southern African country that shares Victoria Falls.",       "Its capital is Harare.",                                    "", "Victoria Falls is the largest sheet of falling water on Earth.", "HARD"  },
    { "ZAMBIA",      "COUNTRIES",  "A landlocked African country north of Zimbabwe.",              "Its capital is Lusaka.",                                    "", "It is named after the mighty Zambezi River.",                  "HARD"   },
    { "BOTSWANA",    "COUNTRIES",  "A southern African country famous for diamonds and elephants.","Its capital is Gaborone.",                                  "", "It has the largest elephant population in the world.",         "HARD"   },
    { "NAMIBIA",     "COUNTRIES",  "A southern African country of giant red sand dunes.",          "Its capital is Windhoek.",                                  "", "It has the oldest desert on Earth.",                           "HARD"   },
    { "SENEGAL",     "COUNTRIES",  "A West African country at the continent's westernmost point.","Its capital is Dakar.",                                     "", "Dakar hosted a famous desert rally for decades.",              "HARD"   },
    { "ALGERIA",     "COUNTRIES",  "The largest country in Africa by area.",                       "Its capital is Algiers.",                                   "", "More than 80 percent of it is Sahara desert.",                 "HARD"   },
    { "TUNISIA",     "COUNTRIES",  "A North African country where ancient Carthage once stood.",   "Star Wars desert scenes were filmed here.",                 "", "It is the northernmost country in Africa.",                    "HARD"   },
    { "LIBYA",       "COUNTRIES",  "A North African country that is mostly desert.",               "Its capital is Tripoli.",                                   "", "It once recorded one of the hottest temperatures on Earth.",   "HARD"   },
    { "SUDAN",       "COUNTRIES",  "A North African country with more pyramids than Egypt.",       "Its capital is Khartoum.",                                  "", "Sudan has over 200 pyramids.",                                 "HARD"   },
    { "MADAGASCAR",  "COUNTRIES",  "A large island off Africa famous for lemurs.",                 "It shares its name with an animated film.",                 "", "Ninety percent of its wildlife is found nowhere else.",        "MEDIUM" },
    { "CUBA",        "COUNTRIES",  "A Caribbean island famous for cigars, salsa and vintage cars.","Its capital is Havana.",                                    "", "Cuba has one of the highest literacy rates in the world.",     "MEDIUM" },
    { "JAMAICA",     "COUNTRIES",  "A Caribbean island famous for reggae music and sprinters.",    "Usain Bolt and Bob Marley came from here.",                 "", "Jamaica was the first Caribbean country to build a railway.",  "MEDIUM" },
    { "HAITI",       "COUNTRIES",  "A Caribbean country that shares an island with the Dominican Republic.","Its capital is Port-au-Prince.",                  "", "It was the first independent Black republic in the world.",    "HARD"   },
    { "PANAMA",      "COUNTRIES",  "A Central American country famous for its canal.",             "Ships cross from one ocean to another here.",               "", "The Panama Canal saves ships a 13,000 km journey.",            "MEDIUM" },
    { "COSTARICA",   "COUNTRIES",  "A Central American country famous for rainforests and no army.","Its motto is pura vida.",                                 "", "It abolished its army in 1948.",                               "HARD"   },
    { "GUATEMALA",   "COUNTRIES",  "A Central American country home to ancient Mayan pyramids.",   "Its capital shares the country's name.",                    "", "Its currency is named after the quetzal bird.",                "HARD"   },
    { "COLOMBIA",    "COUNTRIES",  "A South American country famous for coffee and emeralds.",     "Its capital is Bogotá.",                                    "", "It is the world's largest producer of emeralds.",              "MEDIUM" },
    { "VENEZUELA",   "COUNTRIES",  "A South American country home to the world's highest waterfall.","Its capital is Caracas.",                                "", "Angel Falls drops almost a kilometre.",                        "HARD"   },
    { "ECUADOR",     "COUNTRIES",  "A South American country named after the equator.",            "The Galápagos Islands belong to it.",                       "", "Darwin studied finches on its Galápagos Islands.",             "HARD"   },
    { "BOLIVIA",     "COUNTRIES",  "A landlocked Andean country with the world's largest salt flat.","It has two capitals, La Paz and Sucre.",                  "", "Its salt flat becomes a giant mirror after rain.",             "HARD"   },
    { "PARAGUAY",    "COUNTRIES",  "A landlocked South American country whose capital is Asunción.","It has a huge dam on the Paraná River.",                  "", "Its flag has a different design on each side.",                "HARD"   },
    { "URUGUAY",     "COUNTRIES",  "A small South American country that won the first World Cup.", "Its capital is Montevideo.",                                "", "It hosted and won the very first FIFA World Cup in 1930.",     "HARD"   },
    { "ARGENTINA",   "COUNTRIES",  "A South American country famous for tango, steak and Messi.",  "Its capital is Buenos Aires.",                              "", "It has the widest avenue in the world.",                       "MEDIUM" },
    { "NEWZEALAND",  "COUNTRIES",  "An island nation famous for kiwis, rugby and hobbits.",        "Its capital is Wellington.",                                "", "It was the first country to give women the vote.",             "MEDIUM" },
    { "FIJI",        "COUNTRIES",  "A tropical island nation in the South Pacific.",               "It is famous for its bottled water and coral reefs.",       "", "Fiji is made up of more than 300 islands.",                    "HARD"   },
    { "AMERICA",     "COUNTRIES",  "A large North American country of fifty states.",              "Its capital is Washington and its flag has stars and stripes.","", "It landed the first humans on the Moon.",                    "EASY"   },
    { "BRITAIN",     "COUNTRIES",  "An island nation of England, Scotland and Wales.",             "Its capital is London.",                                    "", "It built the world's first underground railway.",              "EASY"   },

    /* ============================= TECHNOLOGY ========================== */
    { "COMPUTER",    "TECHNOLOGY", "An electronic machine that processes data and runs programs.", "It has a processor and a memory.",                          "", "The first computer bug was a real moth.",                      "EASY"   },
    { "KEYBOARD",    "TECHNOLOGY", "An input device covered in letters, numbers and symbols.",     "You type on it every single day.",                          "", "The QWERTY layout was designed in the 1870s.",                 "EASY"   },
    { "INTERNET",    "TECHNOLOGY", "A global network that connects billions of devices.",          "You use it to open websites.",                              "", "Around five billion people use it today.",                     "EASY"   },
    { "MONITOR",     "TECHNOLOGY", "The screen that shows what your computer is doing.",           "It stands on your desk.",                                   "", "Early ones were deep and very heavy.",                         "EASY"   },
    { "PRINTER",     "TECHNOLOGY", "A device that puts documents onto paper.",                     "It needs ink or toner to work.",                            "", "Three-dimensional ones can print solid objects.",              "EASY"   },
    { "LAPTOP",      "TECHNOLOGY", "A portable computer you can use on your lap.",                 "It has a built-in screen and keyboard.",                    "", "The very first one weighed eleven kilograms.",                 "EASY"   },
    { "MOUSE",       "TECHNOLOGY", "A small handheld device used to point and click.",             "It sits beside the keyboard.",                              "", "It was named after its tail-like cable.",                      "EASY"   },
    { "WEBSITE",     "TECHNOLOGY", "A set of connected pages published on the internet.",          "You are using one right now.",                              "", "The first one went online in 1991.",                           "EASY"   },
    { "BROWSER",     "TECHNOLOGY", "A program used to view pages on the internet.",                "Chrome and Firefox are examples.",                          "", "The first one was called WorldWideWeb.",                       "EASY"   },
    { "WIRELESS",    "TECHNOLOGY", "Working without cables by using radio waves.",                 "Wi-Fi is the best known example.",                          "", "Radio waves travel at the speed of light.",                    "EASY"   },
    { "SOFTWARE",    "TECHNOLOGY", "The programs that tell a machine what to do.",                 "Apps are examples of it.",                                  "", "The word software first appeared in print in 1958.",           "MEDIUM" },
    { "HARDWARE",    "TECHNOLOGY", "The physical parts of a computer that you can touch.",         "It is the opposite of software.",                           "", "A modern phone has more power than early space rockets.",      "MEDIUM" },
    { "ROBOT",       "TECHNOLOGY", "A machine that can be programmed to carry out tasks alone.",   "Factories use them for welding.",                           "", "The word comes from a Czech word meaning forced labour.",      "MEDIUM" },
    { "BLUETOOTH",   "TECHNOLOGY", "A short-range wireless technology for connecting devices.",    "It links headphones to phones.",                            "", "It is named after a tenth-century Danish king.",               "MEDIUM" },
    { "MEMORY",      "TECHNOLOGY", "Where a computer stores its data and programs.",               "It is measured in gigabytes.",                              "", "RAM forgets everything when the power goes off.",              "MEDIUM" },
    { "NETWORK",     "TECHNOLOGY", "A group of computers connected so they can share data.",       "Your school and your home both have one.",                 "", "The very first message sent on one crashed it.",               "MEDIUM" },
    { "PASSWORD",    "TECHNOLOGY", "A secret word used to prove who you are.",                     "You should never share it with anyone.",                    "", "The most common one in the world is 123456.",                  "MEDIUM" },
    { "SMARTPHONE",  "TECHNOLOGY", "A pocket computer that can also make phone calls.",            "It has a touchscreen and apps.",                            "", "More people own one than own a toothbrush.",                   "MEDIUM" },
    { "VIRTUAL",     "TECHNOLOGY", "Existing on a computer instead of in the real world.",         "Reality headsets use this word.",                           "", "Virtual memory lets a computer run more programs at once.",    "MEDIUM" },
    { "MACHINE",     "TECHNOLOGY", "Any device that uses power to carry out work.",                "Learning with it powers modern AI.",                        "", "Simple ones include levers and pulleys.",                      "MEDIUM" },
    { "CODING",      "TECHNOLOGY", "Writing the instructions that programs are made of.",          "It is how this game was built.",                            "", "There are more than 700 programming languages.",               "MEDIUM" },
    { "SERVER",      "TECHNOLOGY", "A computer that provides data to other computers.",            "Every website lives on one.",                               "", "The first one was a rewritten telephone switch.",              "MEDIUM" },
    { "GRAPHICS",    "TECHNOLOGY", "Images and visual content created or shown by a computer.",    "Special cards make games look better.",                    "", "Pictures on a screen are made of tiny pixels.",                "MEDIUM" },
    { "DATABASE",    "TECHNOLOGY", "An organised collection of data stored for fast searching.",   "SQL is the language used to ask it questions.",             "", "The very first ones appeared in the 1960s.",                   "HARD"   },
    { "PROGRAMMING", "TECHNOLOGY", "Writing instructions that a computer can execute.",            "C++ and Python are used for it.",                           "", "The first programmer in history was Ada Lovelace.",            "HARD"   },
    { "ALGORITHM",   "TECHNOLOGY", "A step-by-step set of instructions for solving a problem.",    "Search engines depend on them.",                            "", "The word comes from the mathematician Al-Khwarizmi.",          "HARD"   },
    { "PROCESSOR",   "TECHNOLOGY", "The chip that carries out a computer's instructions.",         "It is called the brain of the computer.",                  "", "Modern ones contain billions of transistors.",                 "HARD"   },
    { "FIREWALL",    "TECHNOLOGY", "A security system that blocks unwanted network traffic.",      "It protects computers from attackers.",                    "", "Its name comes from real fire walls in buildings.",            "HARD"   },
    { "SATELLITE",   "TECHNOLOGY", "An object launched into orbit around the Earth.",              "It helps GPS find your position.",                          "", "Thousands of them are orbiting above us right now.",           "HARD"   },
    { "CIRCUIT",     "TECHNOLOGY", "A complete path that electricity flows along inside a device.","It is built from wires and chips.",                        "", "The circuits in a modern chip are nano-scale.",                "HARD"   },
    { "ENCRYPTION",  "TECHNOLOGY", "Turning data into a code that only the right reader can open.","It keeps private messages secret.",                        "", "It protected messages long before computers existed.",         "HARD"   },
    { "AUTOMATION",  "TECHNOLOGY", "Using machines to do work without human action.",              "Factories use it on assembly lines.",                      "", "The word comes from the Greek for self-moving.",               "HARD"   },
    { "EMAIL",       "TECHNOLOGY", "A digital letter sent over the internet.",                     "It has a subject line and an @ in the address.",            "", "The first one was sent in 1971.",                              "EASY"   },
    { "CAMERA",      "TECHNOLOGY", "A device that captures photos and videos.",                    "Every phone has at least one.",                             "", "The first photograph took eight hours to expose.",             "EASY"   },
    { "TABLET",      "TECHNOLOGY", "A flat touchscreen computer bigger than a phone.",             "You can read and draw on it.",                              "", "The iPad sold 300,000 units on its first day.",                "EASY"   },
    { "SPEAKER",     "TECHNOLOGY", "A device that turns electrical signals into sound.",           "Music comes out of it.",                                    "", "The loudest speakers can damage hearing from far away.",       "EASY"   },
    { "HEADPHONES",  "TECHNOLOGY", "Small speakers you wear over or in your ears.",                "They let you listen without disturbing others.",            "", "They were first made for telephone operators in 1910.",        "MEDIUM" },
    { "CHARGER",     "TECHNOLOGY", "A device that refills the battery of your phone.",             "You plug it into the wall.",                                "", "Wireless ones use magnetic fields to transfer power.",         "EASY"   },
    { "BATTERY",     "TECHNOLOGY", "A device that stores energy to power gadgets.",                "It has a plus and a minus end.",                            "", "The first one was invented by Volta in 1800.",                 "EASY"   },
    { "SCREEN",      "TECHNOLOGY", "The surface where a device displays images.",                  "You touch it on your phone.",                               "", "Modern screens contain millions of tiny coloured dots.",       "EASY"   },
    { "PIXEL",       "TECHNOLOGY", "The smallest dot of colour on a screen.",                      "Millions of them make up a picture.",                       "", "The word comes from picture element.",                         "MEDIUM" },
    { "CURSOR",      "TECHNOLOGY", "The little arrow that moves when you move the mouse.",         "It turns into a hand over a link.",                         "", "The first cursor was a plain vertical line.",                  "MEDIUM" },
    { "FOLDER",      "TECHNOLOGY", "A digital container used to organise files.",                  "Its icon looks like a paper file.",                         "", "Folders are called directories in older systems.",             "EASY"   },
    { "FILE",        "TECHNOLOGY", "A named collection of data stored on a computer.",             "Documents, photos and songs are all this.",                 "", "The first file systems appeared in the 1960s.",                "EASY"   },
    { "DOWNLOAD",    "TECHNOLOGY", "Copying a file from the internet onto your device.",           "The opposite is upload.",                                   "", "Early modems downloaded one photo in several minutes.",        "EASY"   },
    { "UPLOAD",      "TECHNOLOGY", "Sending a file from your device to the internet.",             "You do it when you post a photo.",                          "", "Most home connections upload slower than they download.",      "EASY"   },
    { "BACKUP",      "TECHNOLOGY", "An extra copy of your data kept in case the original is lost.","Smart people make one regularly.",                          "", "World Backup Day is celebrated on 31 March.",                  "MEDIUM" },
    { "CLOUD",       "TECHNOLOGY", "Storing data on remote internet servers instead of your device.","Your photos may be saved there.",                          "", "The cloud is really huge buildings full of computers.",        "EASY"   },
    { "STORAGE",     "TECHNOLOGY", "Space where a device keeps its files and apps.",               "It is measured in gigabytes and terabytes.",                "", "A terabyte can hold about 250,000 photos.",                    "MEDIUM" },
    { "ANTIVIRUS",   "TECHNOLOGY", "Software that protects a computer from malicious programs.",   "It scans files for threats.",                               "", "The first computer virus appeared in 1971.",                   "HARD"   },
    { "MALWARE",     "TECHNOLOGY", "Harmful software designed to damage or steal data.",           "Viruses and spyware are types of it.",                      "", "The name is short for malicious software.",                    "HARD"   },
    { "HACKER",      "TECHNOLOGY", "A person who breaks into computer systems.",                   "Some are good and help find security holes.",               "", "Ethical hackers are paid to find bugs before criminals do.",   "MEDIUM" },
    { "SPAM",        "TECHNOLOGY", "Unwanted junk messages sent to many people.",                  "It fills up your email inbox.",                             "", "Nearly half of all email sent is spam.",                       "EASY"   },
    { "COOKIE",      "TECHNOLOGY", "A small file a website stores on your computer to remember you.","Websites ask you to accept them.",                        "", "They were invented in 1994 for online shopping carts.",        "MEDIUM" },
    { "CACHE",       "TECHNOLOGY", "A temporary store of data that makes things load faster.",     "Clearing it can fix a slow browser.",                       "", "The word comes from the French for hiding place.",             "HARD"   },
    { "BANDWIDTH",   "TECHNOLOGY", "The amount of data a connection can carry per second.",        "More of it means faster streaming.",                        "", "Video streaming uses most of the internet's bandwidth.",       "HARD"   },
    { "MODEM",       "TECHNOLOGY", "A device that connects your home to the internet provider.",   "It often sits next to the router.",                         "", "Early modems screeched loudly when connecting.",               "MEDIUM" },
    { "ROUTER",      "TECHNOLOGY", "A device that shares an internet connection with all your devices.","It creates your home Wi-Fi.",                            "", "A router decides the best path for every packet of data.",     "MEDIUM" },
    { "ETHERNET",    "TECHNOLOGY", "A cable standard for connecting computers to a network.",      "Its plug is bigger than a phone plug.",                     "", "It was invented at Xerox in 1973.",                            "HARD"   },
    { "PROTOCOL",    "TECHNOLOGY", "A set of rules computers follow to talk to each other.",       "HTTP is one used by the web.",                              "", "The internet runs on a protocol suite called TCP/IP.",         "HARD"   },
    { "DOMAIN",      "TECHNOLOGY", "The name of a website, like example dot com.",                 "You type it into the address bar.",                         "", "The first registered domain was symbolics.com in 1985.",       "MEDIUM" },
    { "HYPERLINK",   "TECHNOLOGY", "A clickable connection from one web page to another.",         "It is usually blue and underlined.",                        "", "The concept was described in 1965, before the web existed.",   "HARD"   },
    { "BROWSING",    "TECHNOLOGY", "Looking through pages on the internet.",                       "You do it with Chrome or Firefox.",                         "", "The average person spends almost seven hours a day online.",   "MEDIUM" },
    { "STREAMING",   "TECHNOLOGY", "Watching or listening to media over the internet without downloading it.","Netflix and Spotify do this.",                  "", "Streaming accounts for most of all internet traffic.",         "MEDIUM" },
    { "PODCAST",     "TECHNOLOGY", "A digital audio show you can subscribe to and listen to any time.","It is like radio on demand.",                            "", "The word blends iPod and broadcast.",                          "MEDIUM" },
    { "BLOG",        "TECHNOLOGY", "A website where someone regularly writes posts.",              "It is short for web log.",                                  "", "There are more than 600 million blogs online.",                "EASY"   },
    { "EMOJI",       "TECHNOLOGY", "A small picture used in messages to show feelings.",           "The smiling face is the most popular.",                     "", "The word is Japanese for picture character.",                  "EASY"   },
    { "SELFIE",      "TECHNOLOGY", "A photo you take of yourself with your phone.",                "Front cameras were made for it.",                           "", "It was the Oxford word of the year in 2013.",                  "EASY"   },
    { "HASHTAG",     "TECHNOLOGY", "A word with a # symbol used to group posts online.",           "It started on Twitter.",                                    "", "The # symbol is officially called an octothorpe.",             "MEDIUM" },
    { "AVATAR",      "TECHNOLOGY", "A picture or character that represents you online.",          "You choose one for your profile.",                          "", "The word comes from Sanskrit and means descent.",              "MEDIUM" },
    { "USERNAME",    "TECHNOLOGY", "The name you use to log in to a website or app.",              "It is usually paired with a password.",                     "", "The most common username in the world is admin.",              "EASY"   },
    { "LOGIN",       "TECHNOLOGY", "Entering your details to access an account.",                  "You did it to start this game.",                            "", "Logging in was invented for time-sharing computers in 1961.",  "EASY"   },
    { "APP",         "TECHNOLOGY", "A small program you install on your phone.",                   "You download it from a store.",                             "", "There are more than five million apps to choose from.",        "EASY"   },
    { "UPDATE",      "TECHNOLOGY", "A new version of software that fixes bugs and adds features.", "Your phone asks you to install one.",                       "", "Updates often fix security holes hackers could use.",          "EASY"   },
    { "BUG",         "TECHNOLOGY", "A mistake in a program that makes it behave wrongly.",         "Programmers spend hours fixing them.",                      "", "The term became popular after a real moth was found in a computer.", "EASY" },
    { "DEBUG",       "TECHNOLOGY", "Finding and fixing mistakes in a program.",                    "It is what programmers do most of the day.",                "", "Grace Hopper popularised the term in the 1940s.",              "MEDIUM" },
    { "COMPILER",    "TECHNOLOGY", "A program that turns source code into a runnable program.",    "C++ needs one before the code can run.",                    "", "The first compiler was written by Grace Hopper in 1952.",      "HARD"   },
    { "VARIABLE",    "TECHNOLOGY", "A named box in a program that stores a value.",                "Its value can change while the program runs.",              "", "Choosing good variable names is a big part of clean code.",    "HARD"   },
    { "FUNCTION",    "TECHNOLOGY", "A reusable block of code that performs one task.",             "You call it by its name.",                                  "", "Functions help avoid writing the same code twice.",            "HARD"   },
    { "LOOP",        "TECHNOLOGY", "A programming structure that repeats instructions.",           "For and while are two kinds.",                              "", "An endless one is called an infinite loop.",                   "MEDIUM" },
    { "ARRAY",       "TECHNOLOGY", "A list of values stored together under one name.",             "Its items are numbered starting from zero.",                "", "Arrays are one of the oldest data structures.",                "HARD"   },
    { "BINARY",      "TECHNOLOGY", "The number system of only ones and zeros that computers use.", "Every file is really made of it.",                          "", "Eight binary digits make one byte.",                           "MEDIUM" },
    { "BYTE",        "TECHNOLOGY", "A unit of data made of eight bits.",                           "One letter of text takes about one of these.",              "", "A gigabyte is about one billion of them.",                     "MEDIUM" },
    { "CHIP",        "TECHNOLOGY", "A tiny piece of silicon holding millions of electronic parts.","It is the heart of every device.",                          "", "Modern chips have features smaller than a virus.",             "EASY"   },
    { "SILICON",     "TECHNOLOGY", "The element that computer chips are made from.",               "A famous tech valley is named after it.",                   "", "Silicon is the second most common element in the Earth's crust.", "HARD" },
    { "TRANSISTOR",  "TECHNOLOGY", "A tiny electronic switch inside every chip.",                  "Billions of them fit inside one processor.",                "", "It was invented in 1947 and won a Nobel Prize.",               "HARD"   },
    { "MOTHERBOARD", "TECHNOLOGY", "The main circuit board that connects all parts of a computer.","The processor and memory plug into it.",                    "", "It is called the mother because every other board connects to it.", "HARD" },
    { "GPU",         "TECHNOLOGY", "A chip specialised in drawing graphics and running AI.",       "Gamers care about it a lot.",                               "", "GPUs can perform thousands of calculations at once.",          "HARD"   },
    { "SENSOR",      "TECHNOLOGY", "A device that detects light, heat, motion or sound.",          "Your phone has more than a dozen.",                         "", "A smartphone typically contains about 14 sensors.",            "MEDIUM" },
    { "DRONE",       "TECHNOLOGY", "A small flying machine controlled remotely.",                  "It often carries a camera.",                                "", "Drones are now used to deliver medicine to remote areas.",     "EASY"   },
    { "GPS",         "TECHNOLOGY", "A satellite system that tells you exactly where you are.",     "Maps apps depend on it.",                                   "", "GPS satellites orbit 20,000 kilometres above the Earth.",      "MEDIUM" },
    { "TOUCHSCREEN", "TECHNOLOGY", "A display you control by tapping and swiping with your fingers.","Nearly every phone has one.",                             "", "The first one was invented in 1965.",                          "MEDIUM" },
    { "HOLOGRAM",    "TECHNOLOGY", "A three-dimensional image made with light.",                   "It looks like it floats in the air.",                       "", "Holograms are printed on credit cards to stop copying.",       "HARD"   },
    { "LASER",       "TECHNOLOGY", "A narrow beam of very focused light.",                         "It reads discs and cuts metal.",                            "", "The word is an acronym for light amplification.",              "MEDIUM" },
    { "BARCODE",     "TECHNOLOGY", "A pattern of black lines scanned at shop checkouts.",          "It stores a product number.",                               "", "The first item ever scanned was a pack of chewing gum.",       "MEDIUM" },
    { "QRCODE",      "TECHNOLOGY", "A square pattern of black dots you scan with a phone camera.", "Restaurants use it for menus.",                             "", "It was invented in Japan for tracking car parts.",             "MEDIUM" },
    { "SCANNER",     "TECHNOLOGY", "A device that turns paper documents into digital images.",     "It is often built into a printer.",                         "", "The first image scanner was built in 1957.",                   "EASY"   },
    { "PROJECTOR",   "TECHNOLOGY", "A device that throws a big image onto a wall or screen.",      "Classrooms and cinemas use it.",                            "", "The first movie projectors were hand-cranked.",                "MEDIUM" },
    { "TELEVISION",  "TECHNOLOGY", "A screen that receives broadcast programmes.",                 "People watch shows and news on it.",                        "", "The first TV broadcast happened in 1926.",                     "EASY"   },
    { "REMOTE",      "TECHNOLOGY", "A handheld device that controls a TV from across the room.",   "It is always lost in the sofa.",                            "", "The first one was connected to the TV by a cable.",            "EASY"   },
    { "MICROPHONE",  "TECHNOLOGY", "A device that captures sound and turns it into a signal.",     "Singers hold one on stage.",                                "", "It was invented for the telephone in 1876.",                   "MEDIUM" },
    { "WEBCAM",      "TECHNOLOGY", "A small camera used for video calls.",                         "It sits above your laptop screen.",                         "", "The first one watched a coffee pot at Cambridge University.",  "MEDIUM" },
    { "JOYSTICK",    "TECHNOLOGY", "A stick-shaped controller used for games and planes.",         "You tilt it to steer.",                                     "", "It was originally invented for aircraft in 1908.",             "MEDIUM" },
    { "CONSOLE",     "TECHNOLOGY", "A machine built specially for playing video games.",           "PlayStation and Xbox are examples.",                        "", "The first home console was released in 1972.",                 "MEDIUM" },
    { "RESOLUTION",  "TECHNOLOGY", "The number of dots that make up a screen or image.",           "4K and HD describe it.",                                    "", "A 4K screen has more than eight million pixels.",              "HARD"   },
    { "SPREADSHEET", "TECHNOLOGY", "A program of rows and columns used for calculations.",         "Excel is the most famous one.",                             "", "The first one, VisiCalc, made people buy personal computers.", "HARD"   },
    { "PRESENTATION","TECHNOLOGY", "A set of slides shown to an audience.",                        "PowerPoint is used to make one.",                           "", "About 30 million presentations are made every day.",           "HARD"   },
    { "KEYWORD",     "TECHNOLOGY", "A word you type into a search engine to find something.",      "Choosing good ones gives better results.",                  "", "Google handles billions of them every day.",                   "MEDIUM" },
    { "SEARCH",      "TECHNOLOGY", "Looking for information by typing words into a website.",      "Google is famous for it.",                                  "", "Search engines index hundreds of billions of pages.",          "EASY"   },
    { "EARBUDS",     "TECHNOLOGY", "Tiny wireless speakers that sit inside your ears.",            "They come in a small charging case.",                       "", "The first wireless earbuds went on sale in 2015.",             "MEDIUM" },
    { "GADGET",      "TECHNOLOGY", "A small clever electronic device.",                            "Smart watches and earbuds are examples.",                   "", "The word was first used by sailors in the 1800s.",             "EASY"   },
    { "SMARTWATCH",  "TECHNOLOGY", "A watch that shows messages and tracks your steps.",           "It connects to your phone.",                                "", "Some can detect an irregular heartbeat.",                      "MEDIUM" },
    { "ELECTRICITY", "TECHNOLOGY", "The energy that powers every electronic device.",              "It flows through wires.",                                   "", "It travels at nearly the speed of light.",                     "MEDIUM" },
    { "SIGNAL",      "TECHNOLOGY", "The invisible wave that carries your phone calls.",            "Bars on your phone show its strength.",                     "", "Phone signals travel as radio waves.",                         "EASY"   },
    { "ANTENNA",     "TECHNOLOGY", "A metal device that sends and receives radio signals.",        "Old TVs had one on the roof.",                              "", "Your phone has several tiny antennas inside.",                 "MEDIUM" },
    { "RADAR",       "TECHNOLOGY", "A system that uses radio waves to detect distant objects.",    "Airports use it to track planes.",                          "", "Radar was crucial in the Second World War.",                   "MEDIUM" },
    { "NANOTECHNOLOGY","TECHNOLOGY","Engineering with materials at the scale of atoms.",           "It builds things thousands of times thinner than a hair.",  "", "A nanometre is one billionth of a metre.",                     "HARD"   },
    { "BIOMETRICS",  "TECHNOLOGY", "Using fingerprints or faces to prove identity.",               "Your phone may unlock with it.",                            "", "No two people have the same fingerprints, even twins.",        "HARD"   },
    { "CRYPTOCURRENCY","TECHNOLOGY","Digital money secured by cryptography.",                      "Bitcoin was the first one.",                                "", "The first Bitcoin purchase was two pizzas.",                   "HARD"   },
    { "BLOCKCHAIN",  "TECHNOLOGY", "A shared digital ledger that cannot be secretly changed.",     "Cryptocurrencies run on it.",                               "", "Every block is linked to the previous one by a code.",         "HARD"   },
    { "CHATBOT",     "TECHNOLOGY", "A program that chats with people by text or voice.",           "Websites use it for customer support.",                     "", "The first chatbot, ELIZA, was created in 1966.",               "MEDIUM" },
    { "METAVERSE",   "TECHNOLOGY", "A shared virtual world people visit as avatars.",              "It combines gaming and social media.",                      "", "The word was coined in a 1992 science-fiction novel.",         "HARD"   },
    { "AUGMENTED",   "TECHNOLOGY", "Reality with digital images layered over the real world.",     "Pokémon Go made it famous.",                                "", "Augmented reality glasses can show directions on the road.",   "HARD"   },
    { "SUPERCOMPUTER","TECHNOLOGY","An extremely powerful computer used for science.",             "It predicts weather and models the climate.",               "", "The fastest ones perform a quintillion calculations per second.", "HARD" },
    { "MAINFRAME",   "TECHNOLOGY", "A large powerful computer used by banks and airlines.",        "It processes millions of transactions.",                    "", "Most credit card payments still pass through one.",            "HARD"   },
    { "TERMINAL",    "TECHNOLOGY", "A text screen where you type commands to a computer.",         "Programmers love it.",                                      "", "Before mice existed, every computer was used this way.",       "HARD"   },
    { "OPERATING",   "TECHNOLOGY", "The kind of system, like Windows or Android, that runs a device.","Every computer needs one to start.",                       "", "Linux runs most of the world's servers.",                      "HARD"   },

    /* =============================== SPORTS ============================ */
    { "CRICKET",     "SPORTS",     "A bat-and-ball game played between two teams of eleven.",     "It is hugely popular in India and Australia.",             "", "A test match can last five whole days.",                       "EASY"   },
    { "FOOTBALL",    "SPORTS",     "The most popular sport in the world, played with two goals.", "The World Cup is its biggest event.",                       "", "The final is watched by more than a billion people.",          "EASY"   },
    { "TENNIS",      "SPORTS",     "A racket sport played on grass, clay or hard courts.",         "Wimbledon is its top tournament.",                          "", "One famous match once lasted eleven hours.",                   "EASY"   },
    { "SWIMMING",    "SPORTS",     "A sport of moving through the water as fast as possible.",     "It has freestyle and butterfly strokes.",                  "", "Humans have swum for many thousands of years.",                "EASY"   },
    { "CHESS",       "SPORTS",     "A board game of strategy played by two people.",               "It has kings, queens and pawns.",                           "", "It has more possible games than atoms on Earth.",              "EASY"   },
    { "SPRINT",      "SPORTS",     "A very short race run at top speed.",                          "Usain Bolt is its greatest ever star.",                    "", "The 100 metre race lasts under ten seconds.",                 "EASY"   },
    { "HOCKEY",      "SPORTS",     "A team sport played with sticks and a ball or a puck.",        "India has won many Olympic golds in it.",                  "", "It is India's national game.",                                 "MEDIUM" },
    { "BADMINTON",   "SPORTS",     "A very fast racket sport played with a shuttlecock.",          "You can play it in the garden or at the Olympics.",        "", "A shuttle can leave the racket at 400 km/h.",                  "MEDIUM" },
    { "BASKETBALL",  "SPORTS",     "A sport where teams score by throwing a ball through a hoop.", "The NBA is its top league.",                                "", "It was invented in 1891 as an indoor winter game.",            "MEDIUM" },
    { "VOLLEYBALL",  "SPORTS",     "A team sport where the ball is hit over a net without catching.","It is often played on the beach.",                         "", "It was first called mintonette.",                              "MEDIUM" },
    { "BOXING",      "SPORTS",     "A combat sport fought with gloved fists inside a ring.",       "Muhammad Ali was its most famous name.",                   "", "It was part of the ancient Olympic Games.",                    "MEDIUM" },
    { "CYCLING",     "SPORTS",     "A sport of racing on two wheels.",                             "Its most famous race crosses France.",                      "", "That race covers more than 3,000 kilometres.",                 "MEDIUM" },
    { "ATHLETICS",   "SPORTS",     "A collection of track and field events.",                      "Running, jumping and throwing are part of it.",           "", "Its roots are in ancient Greece.",                             "MEDIUM" },
    { "GOLF",        "SPORTS",     "A sport of hitting a small ball into holes using clubs.",      "Tiger Woods made it famous worldwide.",                    "", "A standard round has eighteen holes.",                         "MEDIUM" },
    { "BASEBALL",    "SPORTS",     "A bat-and-ball sport hugely popular in the USA and Japan.",    "Players try to hit home runs.",                             "", "A game is divided into nine innings.",                         "MEDIUM" },
    { "KABADDI",     "SPORTS",     "A traditional Indian team contact sport of raiding.",          "Players chant its name while raiding.",                    "", "It is the state game of several Indian states.",               "MEDIUM" },
    { "SKATING",     "SPORTS",     "Gliding on ice or on wheels as a sport.",                      "It can be done on a rink or a ramp.",                      "", "Figure skating began on frozen canals.",                       "MEDIUM" },
    { "CLIMBING",    "SPORTS",     "A sport of ascending rock faces or artificial walls.",         "It joined the Olympics in 2020.",                           "", "It is also a very popular indoor hobby.",                      "MEDIUM" },
    { "MARATHON",    "SPORTS",     "A long-distance race of about forty-two kilometres.",          "It is named after a Greek legend.",                        "", "The exact distance was fixed in 1908.",                        "MEDIUM" },
    { "DIVING",      "SPORTS",     "A sport of jumping into water with acrobatic moves.",          "It is scored for form and entry.",                          "", "Olympic dives are rated by their difficulty.",                 "MEDIUM" },
    { "SURFING",     "SPORTS",     "A water sport of riding waves on a board.",                    "It began in Hawaii.",                                      "", "It joined the Olympic Games in 2020.",                         "MEDIUM" },
    { "KARATE",      "SPORTS",     "A Japanese martial art of strikes, kicks and blocks.",         "Its name means empty hand.",                               "", "It is taught in thousands of schools worldwide.",              "MEDIUM" },
    { "ARCHERY",     "SPORTS",     "A sport of shooting arrows at a target with a bow.",           "Robin Hood was famous for it.",                            "", "It has been an Olympic sport since 1900.",                     "HARD"   },
    { "RUGBY",       "SPORTS",     "A contact sport played with an oval ball.",                    "Its World Cup is held every four years.",                  "", "Early balls were made from a pig's bladder.",                  "HARD"   },
    { "WRESTLING",   "SPORTS",     "One of the oldest combat sports in the world.",                "It was part of the ancient Olympics.",                     "", "Cave drawings of it are 15,000 years old.",                    "HARD"   },
    { "ROWING",      "SPORTS",     "A water sport of moving a boat forward with oars.",            "It is an Olympic sport with crews.",                        "", "Oars have been used for more than 5,000 years.",               "HARD"   },
    { "SAILING",     "SPORTS",     "A sport of racing boats using only the power of the wind.",    "It needs a mast and a sail.",                              "", "Its oldest trophy is the America's Cup.",                      "HARD"   },
    { "HURDLES",     "SPORTS",     "A race in which runners must jump over a series of barriers.", "Knocking them over slows you down.",                       "", "A 110 metre race has ten barriers.",                           "HARD"   },
    { "JAVELIN",     "SPORTS",     "A field event of throwing a long spear as far as possible.",   "Neeraj Chopra won Olympic gold in it.",                    "", "The world record is over 98 metres.",                          "HARD"   },
    { "SHOTPUT",     "SPORTS",     "A field event of pushing a heavy metal ball from the shoulder.","The ball is put rather than thrown.",                     "", "The men's shot weighs over seven kilograms.",                  "HARD"   },
    { "GYMNASTICS",  "SPORTS",     "A sport of strength, balance and flexibility routines.",       "It uses beams, bars and mats.",                             "", "Its name comes from the Greek word for exercise.",             "HARD"   },
    { "TRIATHLON",   "SPORTS",     "A race that combines swimming, cycling and running.",          "It has three parts in one event.",                          "", "The Olympic distance is 51.5 kilometres in total.",            "HARD"   },
    { "RUNNING",     "SPORTS",     "The simplest sport of all — moving fast on your feet.",        "Marathons and sprints are both forms of it.",               "", "Humans can outrun almost any animal over long distances.",     "EASY"   },
    { "FENCING",     "SPORTS",     "A sword-fighting sport with thin blades and mesh masks.",      "Points are scored with touches.",                           "", "It is one of only five sports in every modern Olympics.",      "MEDIUM" },
    { "JUDO",        "SPORTS",     "A Japanese martial art of throws and holds.",                  "Its name means the gentle way.",                            "", "It was the first Asian martial art in the Olympics.",          "MEDIUM" },
    { "TAEKWONDO",   "SPORTS",     "A Korean martial art famous for high spinning kicks.",         "Competitors wear chest protectors.",                        "", "It is the national sport of South Korea.",                     "HARD"   },
    { "SKIING",      "SPORTS",     "Sliding down snowy mountains on two long boards.",             "You need poles and warm clothes.",                          "", "Skis have been used for at least 8,000 years.",                "EASY"   },
    { "SNOWBOARDING","SPORTS",     "Riding down snowy slopes on a single wide board.",             "It looks like surfing on snow.",                            "", "It joined the Winter Olympics in 1998.",                       "HARD"   },
    { "TABLETENNIS", "SPORTS",     "A fast indoor game played with small paddles and a light ball.","It is also called ping-pong.",                              "", "The ball can spin at 9,000 rotations per minute.",             "HARD"   },
    { "HANDBALL",    "SPORTS",     "A fast team sport where players throw a ball into a goal.",    "It is played indoors on a court.",                          "", "Players can only hold the ball for three seconds.",            "MEDIUM" },
    { "POLO",        "SPORTS",     "A team sport played on horseback with long mallets.",          "It is called the sport of kings.",                          "", "Polo is one of the oldest team sports, over 2,000 years old.", "MEDIUM" },
    { "SQUASH",      "SPORTS",     "A racket sport played against the walls of a small room.",     "The ball is tiny and bounces very little.",                 "", "It was invented at a school in England in the 1830s.",         "MEDIUM" },
    { "SNOOKER",     "SPORTS",     "A cue sport played on a large green table with coloured balls.","The maximum break is 147.",                                "", "It was invented by British army officers in India.",           "HARD"   },
    { "BILLIARDS",   "SPORTS",     "A family of games played by hitting balls with a cue.",        "Pool and snooker belong to it.",                            "", "Balls were once made of ivory before plastic was invented.",   "HARD"   },
    { "BOWLING",     "SPORTS",     "Rolling a heavy ball to knock down ten pins.",                 "A perfect game scores 300.",                                "", "Bowling is more than 5,000 years old.",                        "EASY"   },
    { "WEIGHTLIFTING","SPORTS",    "A strength sport of lifting a loaded bar overhead.",           "The two lifts are snatch and clean and jerk.",              "", "It has been in the Olympics since 1896.",                      "HARD"   },
    { "SKATEBOARDING","SPORTS",    "Riding and doing tricks on a small board with four wheels.",   "It joined the Olympics in 2020.",                           "", "Skateboarding began with surfers in California.",              "HARD"   },
    { "PARKOUR",     "SPORTS",     "Moving quickly through a city by running, jumping and climbing.","It began in France.",                                     "", "Its name comes from the French word for route.",               "HARD"   },
    { "YOGA",        "SPORTS",     "An ancient Indian practice of poses, breathing and calm.",     "People do it on a mat.",                                    "", "International Yoga Day is celebrated on 21 June.",             "EASY"   },
    { "CHESSBOXING", "SPORTS",     "A sport that alternates rounds of chess and boxing.",          "You can win by checkmate or knockout.",                     "", "It began as an idea in a comic book.",                         "HARD"   },
    { "LACROSSE",    "SPORTS",     "A team sport played with a netted stick and a small ball.",    "It was invented by Native Americans.",                      "", "It is the national summer sport of Canada.",                   "HARD"   },
    { "SOFTBALL",    "SPORTS",     "A cousin of baseball played with a bigger ball on a smaller field.","The pitcher throws underarm.",                           "", "It was first played indoors in 1887.",                         "MEDIUM" },
    { "NETBALL",     "SPORTS",     "A team sport like basketball but without dribbling.",          "It is very popular in Australia and England.",              "", "It developed from early women's basketball.",                  "MEDIUM" },
    { "CANOEING",    "SPORTS",     "Paddling a narrow boat with a single-bladed paddle.",          "Slalom races run through white water.",                     "", "The canoe is one of the oldest boat designs.",                 "MEDIUM" },
    { "KAYAKING",    "SPORTS",     "Paddling a closed boat with a double-bladed paddle.",          "It was invented by the Inuit for hunting.",                 "", "Kayak means hunter's boat.",                                   "MEDIUM" },
    { "WATERPOLO",   "SPORTS",     "A team ball sport played in a swimming pool.",                 "Players tread water for the whole game.",                   "", "Players swim up to five kilometres in a single match.",        "HARD"   },
    { "SNORKELING",  "SPORTS",     "Swimming face down with a breathing tube to watch the reef.",  "You wear a mask and fins.",                                 "", "Snorkels were used by ancient Greek sponge divers.",           "HARD"   },
    { "MOTORSPORT",  "SPORTS",     "Racing with cars or motorcycles.",                             "Formula One is its most famous series.",                    "", "F1 cars can brake from 100 km/h to zero in 17 metres.",        "HARD"   },
    { "FORMULAONE",  "SPORTS",     "The fastest car racing championship in the world.",            "Its cars are single-seaters with huge wings.",              "", "A pit stop can change four tyres in under two seconds.",       "HARD"   },
    { "HORSERACING", "SPORTS",     "Jockeys racing horses around a track.",                        "The Derby is its most famous race.",                        "", "It is one of the oldest sports still practised.",              "HARD"   },
    { "EQUESTRIAN",  "SPORTS",     "Olympic sports performed on horseback.",                       "Jumping and dressage are two forms.",                       "", "It is the only Olympic sport where men and women compete equally.", "HARD" },
    { "SHOOTING",    "SPORTS",     "A precision sport of hitting targets with rifles or pistols.", "It needs a very steady hand.",                              "", "Shooting has been in the Olympics since the first Games.",     "MEDIUM" },
    { "CURLING",     "SPORTS",     "Sliding heavy stones across ice towards a target.",            "Team-mates sweep the ice with brooms.",                     "", "It is nicknamed chess on ice.",                                "MEDIUM" },
    { "BOBSLEIGH",   "SPORTS",     "Racing down an icy track in a sled at high speed.",            "Teams of two or four push and jump in.",                    "", "Bobsleighs can reach 150 km/h.",                               "HARD"   },
    { "LUGE",        "SPORTS",     "Sliding feet-first down an ice track on a tiny sled.",         "It is one of the fastest Winter Olympic sports.",           "", "Luge athletes steer with their calves and shoulders.",         "HARD"   },
    { "BIATHLON",    "SPORTS",     "A winter sport combining cross-country skiing and rifle shooting.","Athletes must calm their heartbeat to shoot.",           "", "It grew out of military training in Scandinavia.",             "HARD"   },
    { "ICEHOCKEY",   "SPORTS",     "A fast sport played on ice with sticks and a puck.",           "It is the national winter sport of Canada.",                "", "A hockey puck can travel at 160 km/h.",                        "HARD"   },
    { "FIGURESKATING","SPORTS",    "Skating on ice with jumps, spins and artistic routines.",      "Skaters perform to music.",                                 "", "It was the first winter sport included in the Olympics.",      "HARD"   },
    { "SPEEDSKATING","SPORTS",     "Racing on ice around an oval track on long blades.",           "Skaters lean low and swing their arms.",                    "", "The Dutch have won more medals in it than any nation.",        "HARD"   },
    { "CROSSCOUNTRY","SPORTS",     "Long-distance running over fields, hills and mud.",            "Races are held in autumn and winter.",                      "", "It was an Olympic sport until 1924.",                          "HARD"   },
    { "STEEPLECHASE","SPORTS",     "A track race with hurdles and a water jump.",                  "Runners get very wet.",                                     "", "It was inspired by horse races between church steeples.",      "HARD"   },
    { "DECATHLON",   "SPORTS",     "An athletics event made up of ten different disciplines.",     "Its winner is called the world's greatest athlete.",       "", "It takes two full days to complete.",                          "HARD"   },
    { "HEPTATHLON",  "SPORTS",     "A women's athletics event of seven disciplines.",              "It includes hurdles, high jump and javelin.",               "", "Jessica Ennis won it at the 2012 Olympics.",                   "HARD"   },
    { "HIGHJUMP",    "SPORTS",     "Leaping over a horizontal bar without knocking it off.",       "Athletes go over backwards.",                               "", "The backwards technique is called the Fosbury Flop.",          "MEDIUM" },
    { "LONGJUMP",    "SPORTS",     "Sprinting down a runway and leaping into a sand pit.",         "Distance is measured from the take-off board.",             "", "Bob Beamon's 1968 jump was not beaten for 23 years.",          "MEDIUM" },
    { "POLEVAULT",   "SPORTS",     "Using a long flexible pole to launch over a very high bar.",   "Athletes fly six metres into the air.",                     "", "Early poles were made of bamboo.",                             "HARD"   },
    { "DISCUS",      "SPORTS",     "Spinning around to throw a heavy flat disc as far as possible.","Ancient Greek statues show athletes throwing it.",         "", "The discus throw dates back to the ancient Olympics.",         "MEDIUM" },
    { "HAMMER",      "SPORTS",     "Throwing a heavy ball on a wire by spinning in a circle.",     "It is a field event, not a tool.",                          "", "The hammer throw began in Scotland.",                          "MEDIUM" },
    { "RELAY",       "SPORTS",     "A race where team-mates pass a baton to one another.",         "Dropping the baton means disaster.",                        "", "The 4x100 metre relay is one of the most exciting Olympic events.", "EASY" },
    { "HURDLING",    "SPORTS",     "Running fast while leaping over a row of barriers.",           "Rhythm between the barriers is everything.",                "", "Top hurdlers take exactly three strides between hurdles.",     "HARD"   },
    { "RACEWALKING", "SPORTS",     "Walking as fast as possible without ever running.",            "One foot must always touch the ground.",                    "", "Race walkers can cover 50 km in under four hours.",            "HARD"   },
    { "JOGGING",     "SPORTS",     "Running at a slow, gentle pace for fitness.",                  "People do it in the park each morning.",                    "", "Jogging became a fitness craze in the 1970s.",                 "EASY"   },
    { "HIKING",      "SPORTS",     "Walking long distances through hills and nature.",             "You carry a backpack and wear boots.",                      "", "The longest hiking trail is over 24,000 kilometres.",          "EASY"   },
    { "CAMPING",     "SPORTS",     "Sleeping outdoors in a tent as an adventure.",                 "You cook over a campfire.",                                 "", "Camping became a popular hobby in the 1900s.",                 "EASY"   },
    { "FISHING",     "SPORTS",     "Catching fish with a rod, line and hook.",                     "Patience is the most important skill.",                     "", "Fishing is one of the most popular pastimes worldwide.",       "EASY"   },
    { "SKYDIVING",   "SPORTS",     "Jumping from a plane and opening a parachute.",                "You free-fall for about a minute.",                         "", "Skydivers reach speeds of 200 km/h in free fall.",             "MEDIUM" },
    { "PARAGLIDING", "SPORTS",     "Flying with a fabric wing after running off a hill.",          "Pilots can stay up for hours on warm air.",                 "", "Paragliders have flown more than 500 kilometres in one flight.", "HARD" },
    { "BUNGEE",      "SPORTS",     "Jumping from a great height attached to an elastic cord.",     "You bounce back up after the fall.",                        "", "It was inspired by land-diving rituals in Vanuatu.",           "MEDIUM" },
    { "RAFTING",     "SPORTS",     "Riding an inflatable boat down fast rivers.",                  "Everyone paddles through the rapids together.",             "", "Rapids are graded from class one to class six.",               "MEDIUM" },
    { "WINDSURFING", "SPORTS",     "Standing on a board and steering with a sail.",                "It combines surfing and sailing.",                          "", "Windsurfers can exceed 90 km/h.",                              "HARD"   },
    { "KITESURFING", "SPORTS",     "Riding a board over water while pulled by a huge kite.",       "Riders jump many metres into the air.",                     "", "The sport only became popular in the late 1990s.",             "HARD"   },
    { "WAKEBOARDING","SPORTS",     "Riding a board while being towed behind a motorboat.",         "It mixes water skiing and snowboarding.",                   "", "Wakeboards have fins like a surfboard.",                       "HARD"   },
    { "SCUBADIVING", "SPORTS",     "Swimming deep underwater with an air tank.",                   "Divers explore reefs and shipwrecks.",                      "", "SCUBA stands for self-contained underwater breathing apparatus.", "HARD" },
    { "SKATEPARK",   "SPORTS",     "A place with ramps and rails built for skateboarding tricks.", "Riders drop into big bowls.",                               "", "The first purpose-built one opened in 1965.",                  "HARD"   },
    { "DODGEBALL",   "SPORTS",     "A game where players throw balls to hit opponents.",           "Getting hit means you are out.",                            "", "There is a real Dodgeball World Championship.",                "MEDIUM" },
    { "KHOKHO",      "SPORTS",     "A traditional Indian chasing game played in teams.",           "Chasers sit in a row and tag runners.",                     "", "It is one of the oldest outdoor games of the Indian subcontinent.", "HARD" },
    { "CARROM",      "SPORTS",     "A tabletop game of flicking discs into corner pockets.",       "It is played with a striker and powder.",                   "", "Carrom originated in India centuries ago.",                    "MEDIUM" },
    { "DARTS",       "SPORTS",     "Throwing small pointed arrows at a round numbered board.",     "The bullseye is in the centre.",                            "", "Modern dartboards are made from compressed sisal fibres.",     "EASY"   },
    { "PICKLEBALL",  "SPORTS",     "A paddle sport that mixes tennis, badminton and ping-pong.",   "It is the fastest growing sport in America.",               "", "It was invented in 1965 on a family's backyard court.",        "HARD"   },
    { "CROQUET",     "SPORTS",     "Hitting balls through hoops on a lawn with a mallet.",         "It appears in Alice in Wonderland.",                        "", "It was an Olympic sport only once, in 1900.",                  "HARD"   },
    { "BASEJUMPING", "SPORTS",     "Parachuting from buildings, bridges or cliffs.",               "It is one of the most dangerous sports.",                   "", "BASE stands for building, antenna, span and earth.",           "HARD"   },
    { "ORIENTEERING","SPORTS",     "Racing through unfamiliar land using only a map and compass.", "Runners must find checkpoints in order.",                   "", "It started as military training in Sweden.",                   "HARD"   },
    { "POWERLIFTING","SPORTS",     "A strength sport of squat, bench press and deadlift.",         "Lifters try for the heaviest single lift.",                 "", "The heaviest deadlift ever is over 500 kilograms.",            "HARD"   },
    { "BODYBUILDING","SPORTS",     "Training to build large, defined muscles for competition.",    "Competitors pose on stage.",                                "", "Arnold Schwarzenegger won Mr Olympia seven times.",            "HARD"   },
    { "CHEERLEADING","SPORTS",     "Energetic routines of jumps, stunts and chants.",              "Squads perform at sports games.",                           "", "It began with male students at Princeton in the 1880s.",       "HARD"   },
    { "TUGOFWAR",    "SPORTS",     "Two teams pulling opposite ends of a rope.",                   "The losing team is dragged over a line.",                   "", "It was an Olympic event from 1900 to 1920.",                   "MEDIUM" },
    { "TRAMPOLINE",  "SPORTS",     "Bouncing high and performing flips on a springy bed.",         "It became an Olympic sport in 2000.",                       "", "Trampolining was originally used to train astronauts.",        "HARD"   },
    { "SUMO",        "SPORTS",     "A Japanese wrestling sport between very large athletes.",      "You lose if you leave the ring or touch the ground.",       "", "Sumo wrestlers eat a special stew called chanko-nabe.",        "MEDIUM" },
    { "KICKBOXING",  "SPORTS",     "A combat sport that allows both punches and kicks.",           "It combines boxing and karate.",                            "", "Modern kickboxing began in Japan in the 1960s.",               "HARD"   },
    { "MUAYTHAI",    "SPORTS",     "The national combat sport of Thailand using eight limbs.",     "Fighters use elbows and knees too.",                        "", "It is called the art of eight limbs.",                         "HARD"   },
    { "AIKIDO",      "SPORTS",     "A Japanese martial art that redirects an attacker's energy.",  "It focuses on throws and joint locks.",                     "", "Its name means the way of harmonious spirit.",                 "HARD"   },
    { "KUNGFU",      "SPORTS",     "Chinese martial arts made famous by Bruce Lee.",               "It has hundreds of different styles.",                      "", "Shaolin monks have practised it for 1,500 years.",             "MEDIUM" },
    { "CAPOEIRA",    "SPORTS",     "A Brazilian martial art that looks like dancing.",             "It is performed to music in a circle.",                     "", "It was created by enslaved Africans in Brazil.",               "HARD"   },
    { "ARMWRESTLING","SPORTS",     "Two people trying to push each other's arm down onto a table.","Elbows must stay on the pad.",                              "", "There are professional arm-wrestling leagues.",                "HARD"   },
    { "PENTATHLON",  "SPORTS",     "An event of five sports: fencing, swimming, riding, shooting and running.","It tests the skills of a soldier.",               "", "It was invented for the 1912 Olympics.",                       "HARD"   },
    { "ULTIMATE",    "SPORTS",     "A team sport played by throwing a flying disc into an end zone.","There are no referees; players call their own fouls.",    "", "It is also known as ultimate frisbee.",                        "HARD"   },

    /* =============================== MOVIES ============================ */
    { "AVATAR",      "MOVIES",     "A science fiction film about blue aliens on the moon Pandora.","James Cameron directed it.",                              "", "It held the box office record for ten years.",                "EASY"   },
    { "TITANIC",     "MOVIES",     "An epic romance about a famous ship that sank in 1912.",       "It stars Leonardo DiCaprio.",                              "", "The real ship was the largest afloat at the time.",            "EASY"   },
    { "FROZEN",      "MOVIES",     "An animated film about two royal sisters and endless winter.", "Its famous song is Let It Go.",                             "", "That song won an Academy Award.",                             "EASY"   },
    { "MOANA",       "MOVIES",     "An animated ocean adventure about a brave island girl.",       "It features the demigod Maui.",                            "", "Brand new water tools were built to animate its sea.",        "EASY"   },
    { "SPIDERMAN",   "MOVIES",     "A superhero film about a teenager bitten by a spider.",        "His uncle taught him about responsibility.",               "", "The character first appeared in comics in 1962.",              "EASY"   },
    { "BATMAN",      "MOVIES",     "A superhero film about a masked hero of Gotham City.",         "He drives a vehicle called the Batmobile.",                "", "He is also known as the Dark Knight.",                        "EASY"   },
    { "SUPERMAN",    "MOVIES",     "A superhero film about a hero sent from the planet Krypton.",  "He flies and wears a red cape.",                            "", "The symbol on his chest stands for hope.",                     "EASY"   },
    { "ROCKY",       "MOVIES",     "A film about an underdog boxer from Philadelphia.",            "He trains by running up museum steps.",                    "", "It won the Best Picture Oscar in 1977.",                       "EASY"   },
    { "SHREK",       "MOVIES",     "An animated comedy about a grumpy green ogre.",                "His best friend is a talking donkey.",                     "", "It won the very first animated film Oscar.",                  "EASY"   },
    { "ALADDIN",     "MOVIES",     "An animated film about a poor boy and a magic lamp.",          "A genie grants him three wishes.",                          "", "Robin Williams voiced the genie.",                             "EASY"   },
    { "CINDERELLA",  "MOVIES",     "A fairy tale film about a girl and a lost slipper.",           "She leaves a glass shoe behind at midnight.",             "", "The oldest known version is from ancient Egypt.",              "EASY"   },
    { "MULAN",       "MOVIES",     "An animated film about a girl who joins the army.",            "She disguises herself as a soldier.",                      "", "It is based on an old Chinese legend.",                        "EASY"   },
    { "INCEPTION",   "MOVIES",     "A mind-bending film about stealing secrets inside dreams.",    "It ends with a spinning top.",                              "", "Christopher Nolan wrote and directed it.",                    "MEDIUM" },
    { "GLADIATOR",   "MOVIES",     "A historical epic about a Roman fighter seeking justice.",     "It is set in ancient Rome.",                                "", "It won five Academy Awards.",                                 "MEDIUM" },
    { "JUMANJI",     "MOVIES",     "An adventure film about a magical board game that comes alive.","Robin Williams starred in it.",                           "", "It is based on a 1981 picture book.",                          "MEDIUM" },
    { "IRONMAN",     "MOVIES",     "A superhero film about a genius inside a metal suit.",         "Robert Downey Jr plays him.",                              "", "It launched the whole Marvel film universe.",                  "MEDIUM" },
    { "HULK",        "MOVIES",     "A superhero film about a scientist who turns green.",          "You would not like him when he is angry.",                 "", "The character first appeared in 1962.",                       "MEDIUM" },
    { "THOR",        "MOVIES",     "A superhero film about the Norse god of thunder.",             "He carries a magic hammer.",                                "", "The hammer is called Mjolnir.",                               "MEDIUM" },
    { "ALIEN",       "MOVIES",     "A science fiction horror film set on a spaceship.",            "In space no one can hear you scream.",                     "", "The creature was designed by H. R. Giger.",                   "MEDIUM" },
    { "PREDATOR",    "MOVIES",     "An action sci-fi film about an invisible alien hunter.",       "Arnold Schwarzenegger fights it in a jungle.",            "", "The hunter sees the world in heat vision.",                    "MEDIUM" },
    { "MADMAX",      "MOVIES",     "A post-apocalyptic film series about desert road wars.",       "It is set in a dusty wasteland.",                           "", "Its fourth film won six Oscars.",                              "MEDIUM" },
    { "KINGKONG",    "MOVIES",     "A monster film about a giant ape on a mysterious island.",     "He climbs a very tall building.",                           "", "The original film was made in 1933.",                         "MEDIUM" },
    { "TRANSFORMERS","MOVIES",     "A sci-fi film about robots that change into vehicles.",        "The Autobots fight the Decepticons.",                      "", "It is based on a popular toy line.",                          "MEDIUM" },
    { "TERMINATOR",  "MOVIES",     "A sci-fi film about a killer robot sent back in time.",        "Its famous line is I'll be back.",                          "", "It created one of cinema's best known characters.",            "MEDIUM" },
    { "TWILIGHT",    "MOVIES",     "A romance film series about vampires and werewolves.",         "It is based on a set of novels.",                           "", "The series earned over three billion dollars.",                "MEDIUM" },
    { "ZOOTOPIA",    "MOVIES",     "An animated film about a rabbit who becomes a police officer.","It is set in a city of animals.",                          "", "It won the Academy Award for animated feature.",               "MEDIUM" },
    { "PINOCCHIO",   "MOVIES",     "An animated film about a wooden boy whose nose grows.",        "A little cricket gives him advice.",                        "", "It was Disney's second animated feature film.",                "MEDIUM" },
    { "INTERSTELLAR","MOVIES",     "A space epic about saving humanity through a wormhole.",       "It features an enormous black hole.",                       "", "Its black hole was modelled on real physics.",                 "HARD"   },
    { "MATRIX",      "MOVIES",     "A science fiction film about a simulated reality.",            "It offers a red pill or a blue pill.",                      "", "Its bullet-time effect changed action cinema.",                "HARD"   },
    { "GODFATHER",   "MOVIES",     "A classic crime film about an Italian-American family.",       "It tells the story of the mafia.",                          "", "It is often called the greatest film ever made.",              "HARD"   },
    { "GODZILLA",    "MOVIES",     "A monster film about a giant creature that rises from the sea.","It first appeared in Japan.",                            "", "It was created as a symbol of nuclear fear.",                  "HARD"   },
    { "JURASSIC",    "MOVIES",     "A film about a park full of cloned dinosaurs.",                "Steven Spielberg directed it.",                            "", "Its dinosaurs were models combined with early CGI.",           "HARD"   },
    { "TOYSTORY",    "MOVIES",     "An animated film about toys that come alive when no one is looking.","Woody and Buzz Lightyear star in it.",                  "", "It was the first fully computer-animated feature film.",       "EASY"   },
    { "NEMO",        "MOVIES",     "An animated film about a lost clownfish and his worried father.","A forgetful blue fish named Dory helps.",                  "", "Clownfish sales rose sharply after its release.",              "EASY"   },
    { "LIONKING",    "MOVIES",     "An animated film about a young lion who must become king.",    "Its villain is his uncle Scar.",                            "", "It was inspired by Shakespeare's Hamlet.",                     "EASY"   },
    { "TANGLED",     "MOVIES",     "An animated film about a princess with very long magic hair.", "She is locked in a tower by Mother Gothel.",                "", "Rapunzel's hair is about 21 metres long.",                     "EASY"   },
    { "COCO",        "MOVIES",     "An animated film about a boy who visits the Land of the Dead.","It celebrates the Mexican Day of the Dead.",                "", "Its song Remember Me won an Oscar.",                           "EASY"   },
    { "ENCANTO",     "MOVIES",     "An animated film about a magical family in Colombia.",         "Everyone has a gift except Mirabel.",                       "", "Its song about Bruno topped the charts worldwide.",            "MEDIUM" },
    { "RATATOUILLE", "MOVIES",     "An animated film about a rat who dreams of being a chef.",     "It is set in a Paris restaurant.",                          "", "Real chefs consulted on every kitchen scene.",                 "HARD"   },
    { "MADAGASCAR",  "MOVIES",     "An animated film about zoo animals who escape to an island.",  "Its penguins became stars of their own films.",             "", "Its lemur king sings I Like to Move It.",                      "MEDIUM" },
    { "MINIONS",     "MOVIES",     "An animated film about small yellow creatures who serve villains.","They speak a made-up language.",                          "", "Their language mixes Spanish, Italian and English.",           "EASY"   },
    { "SING",        "MOVIES",     "An animated film about animals competing in a singing contest.","A koala runs the theatre.",                                "", "It features more than 60 famous songs.",                       "EASY"   },
    { "CARS",        "MOVIES",     "An animated film about a racing car who learns humility in a small town.","Lightning McQueen is its hero.",                  "", "The town of Radiator Springs is based on Route 66.",           "EASY"   },
    { "BOLT",        "MOVIES",     "An animated film about a TV star dog who believes he has super powers.","He travels across America to find his owner.",     "", "It was Disney's first film in digital 3D.",                    "MEDIUM" },
    { "DUMBO",       "MOVIES",     "An animated film about a baby elephant who can fly with his big ears.","He is teased at the circus.",                       "", "It is one of Disney's shortest animated films.",               "EASY"   },
    { "BAMBI",       "MOVIES",     "An animated film about a young deer growing up in the forest.","His friends are a rabbit and a skunk.",                     "", "Walt Disney kept real deer in the studio for the animators.",  "EASY"   },
    { "PETERPAN",    "MOVIES",     "An animated film about a boy who never grows up.",             "He flies to Neverland with Tinker Bell.",                   "", "The story began as a stage play in 1904.",                     "MEDIUM" },
    { "HERCULES",    "MOVIES",     "An animated film about a Greek hero who must prove himself.",  "Hades is the villain.",                                     "", "The film's gospel-style songs were a first for Disney.",       "MEDIUM" },
    { "TARZAN",      "MOVIES",     "An animated film about a boy raised by gorillas in the jungle.","He swings through the trees on vines.",                    "", "Phil Collins wrote and sang its songs.",                       "MEDIUM" },
    { "POCAHONTAS",  "MOVIES",     "An animated film about a Native American woman and an English settler.","Its song is Colors of the Wind.",                   "", "It was the first Disney film based on a real person.",         "HARD"   },
    { "MEGAMIND",    "MOVIES",     "An animated film about a supervillain who accidentally wins.", "He has a big blue head.",                                   "", "Will Ferrell voiced the title character.",                     "HARD"   },
    { "KUNGFUPANDA", "MOVIES",     "An animated film about a clumsy panda who becomes a warrior.", "His name is Po.",                                           "", "The film was praised for its respectful use of Chinese culture.", "HARD" },
    { "ANTMAN",      "MOVIES",     "A superhero film about a thief who can shrink to insect size.","He can also grow to giant size.",                           "", "The character first appeared in comics in 1962.",              "MEDIUM" },
    { "AQUAMAN",     "MOVIES",     "A superhero film about the king of the underwater city Atlantis.","He can talk to sea creatures.",                          "", "Jason Momoa played the title role.",                           "MEDIUM" },
    { "WONDERWOMAN", "MOVIES",     "A superhero film about an Amazon warrior princess.",           "She carries a lasso of truth.",                             "", "The character was created in 1941.",                           "HARD"   },
    { "DEADPOOL",    "MOVIES",     "A superhero comedy about a joking mercenary who cannot die.",  "He often talks directly to the audience.",                  "", "The film was one of the most profitable R-rated movies ever.", "MEDIUM" },
    { "BLACKPANTHER","MOVIES",     "A superhero film about the king of the hidden nation Wakanda.","Its metal vibranium is the strongest on Earth.",           "", "It was the first superhero film nominated for Best Picture.",  "HARD"   },
    { "AVENGERS",    "MOVIES",     "A superhero film where Earth's mightiest heroes team up.",     "Iron Man, Thor and Hulk are members.",                      "", "Endgame was once the highest-grossing film of all time.",      "MEDIUM" },
    { "VENOM",       "MOVIES",     "A film about a journalist bonded to an alien symbiote.",       "The creature has a very long tongue.",                      "", "Tom Hardy also voiced the symbiote.",                          "MEDIUM" },
    { "JOKER",       "MOVIES",     "A dark film about the origin of Batman's greatest enemy.",     "Its star won the Best Actor Oscar.",                        "", "It was the first R-rated film to earn a billion dollars.",     "MEDIUM" },
    { "SHAZAM",      "MOVIES",     "A superhero film about a boy who becomes an adult hero by saying a magic word.","The word is the film's title.",           "", "The character was once more popular than Superman.",           "HARD"   },
    { "LOGAN",       "MOVIES",     "A gritty film about an aging Wolverine protecting a young mutant.","It was Hugh Jackman's farewell to the role.",           "", "It was nominated for a screenplay Oscar, rare for a superhero film.", "HARD" },
    { "ROBOCOP",     "MOVIES",     "A sci-fi film about a police officer rebuilt as a cyborg.",    "It is set in a crime-ridden future Detroit.",              "", "The suit was so heavy the actor lost weight filming.",         "HARD"   },
    { "STARWARS",    "MOVIES",     "A space saga of Jedi, lightsabers and the Force.",             "Its villain breathes heavily and wears black.",             "", "The opening crawl was inspired by 1930s film serials.",        "EASY"   },
    { "STARTREK",    "MOVIES",     "A science-fiction franchise about the starship Enterprise.",   "Its crew boldly goes where no one has gone before.",        "", "It inspired the invention of the flip phone.",                 "MEDIUM" },
    { "GRAVITY",     "MOVIES",     "A film about two astronauts stranded in orbit after an accident.","Sandra Bullock plays the lead.",                          "", "It won seven Academy Awards.",                                 "MEDIUM" },
    { "MARTIAN",     "MOVIES",     "A film about an astronaut left behind on Mars who grows potatoes.","Matt Damon plays the stranded botanist.",               "", "NASA helped make the science as accurate as possible.",        "HARD"   },
    { "ARRIVAL",     "MOVIES",     "A film about a linguist trying to talk to aliens.",            "The aliens write in circles.",                              "", "A real linguist designed the alien language.",                 "HARD"   },
    { "DUNE",        "MOVIES",     "A sci-fi epic about a desert planet and giant sandworms.",     "Its precious substance is called spice.",                   "", "The novel it is based on was rejected by 20 publishers.",      "MEDIUM" },
    { "TENET",       "MOVIES",     "A spy thriller where time can move backwards.",                "Its title reads the same forwards and backwards.",          "", "A real Boeing 747 was crashed for one scene.",                 "HARD"   },
    { "OPPENHEIMER", "MOVIES",     "A biographical film about the scientist who built the atomic bomb.","It won Best Picture in 2024.",                          "", "It was shot partly in black-and-white IMAX film.",             "HARD"   },
    { "BARBIE",      "MOVIES",     "A comedy about a famous doll who leaves her perfect pink world.","Margot Robbie plays the lead.",                            "", "It caused a worldwide shortage of pink paint.",                "EASY"   },
    { "MINECRAFT",   "MOVIES",     "A film based on the world's best-selling block-building game.", "Jack Black plays Steve.",                                   "", "The game has sold more than 300 million copies.",              "MEDIUM" },
    { "SONIC",       "MOVIES",     "A film about a super-fast blue hedgehog from a video game.",   "Jim Carrey plays Dr Robotnik.",                             "", "The character was redesigned after fans disliked the trailer.", "EASY"  },
    { "MARIO",       "MOVIES",     "An animated film about a plumber saving the Mushroom Kingdom.","His brother Luigi wears green.",                            "", "It became the highest-grossing video game film ever.",         "EASY"   },
    { "PIRATES",     "MOVIES",     "An adventure series about a witty pirate captain and cursed treasure.","Captain Jack Sparrow leads the way.",                "", "It is based on a Disneyland theme-park ride.",                 "MEDIUM" },
    { "INDIANAJONES","MOVIES",     "An adventure series about a whip-cracking archaeologist.",     "He hates snakes and wears a fedora.",                       "", "His famous hat was inspired by 1930s adventure serials.",      "HARD"   },
    { "JAWS",        "MOVIES",     "A thriller about a giant shark terrorising a beach town.",     "Its two-note theme is world famous.",                       "", "The mechanical shark kept breaking, so it appears very little.", "EASY" },
    { "ROCKYBALBOA", "MOVIES",     "A boxing film about an underdog fighter's final comeback.",    "He trains by punching frozen meat.",                        "", "Sylvester Stallone wrote the original script in three days.",  "HARD"   },
    { "CREED",       "MOVIES",     "A boxing film about the son of Apollo Creed trained by Rocky.","Michael B. Jordan plays the young fighter.",                "", "The series revived the Rocky franchise for a new generation.", "HARD"   },
    { "FORRESTGUMP", "MOVIES",     "A film about a kind-hearted man who stumbles through history.","Life is like a box of chocolates.",                         "", "Tom Hanks was not paid up front and instead took a share of profits.", "HARD" },
    { "CASTAWAY",    "MOVIES",     "A film about a man stranded on a desert island for years.",    "His only friend is a volleyball named Wilson.",             "", "Filming stopped for a year so the actor could lose weight.",   "MEDIUM" },
    { "HOMEALONE",   "MOVIES",     "A comedy about a boy left behind at Christmas who outsmarts burglars.","He sets traps all over the house.",                  "", "It was the highest-grossing comedy for over 20 years.",        "EASY"   },
    { "MATILDA",     "MOVIES",     "A film about a brilliant girl with telekinetic powers.",       "She battles the terrifying headmistress Miss Trunchbull.", "", "It is based on a book by Roald Dahl.",                         "MEDIUM" },
    { "PADDINGTON",  "MOVIES",     "A film about a polite bear from Peru who loves marmalade.",    "He arrives at a London railway station.",                   "", "It is considered one of the best-reviewed films ever.",        "MEDIUM" },
    { "HARRYPOTTER", "MOVIES",     "A film series about a boy wizard at a magical school.",        "He has a lightning-shaped scar.",                           "", "The films were shot over ten years with the same young cast.", "MEDIUM" },
    { "NARNIA",      "MOVIES",     "A fantasy film about children who enter a magic land through a wardrobe.","A great lion named Aslan rules it.",             "", "It is based on the books of C. S. Lewis.",                     "MEDIUM" },
    { "HOBBIT",      "MOVIES",     "A fantasy film about a small creature on a quest to reclaim a dragon's gold.","Bilbo Baggins is its hero.",                 "", "The films were shot at 48 frames per second.",                 "MEDIUM" },
    { "LORDOFTHERINGS","MOVIES",   "A fantasy trilogy about a quest to destroy a powerful ring.",  "It was filmed entirely in New Zealand.",                    "", "The final film won all eleven Oscars it was nominated for.",   "HARD"   },
    { "MALEFICENT",  "MOVIES",     "A fantasy film that retells Sleeping Beauty from the villain's view.","Angelina Jolie plays the winged fairy.",              "", "Her prosthetic cheekbones were designed by a horror artist.",  "HARD"   },
    { "JUNGLEBOOK",  "MOVIES",     "A film about a boy raised by wolves in the Indian jungle.",    "His friends are a bear and a panther.",                     "", "The 2016 version was filmed almost entirely in a studio.",     "MEDIUM" },
    { "LIFEOFPI",    "MOVIES",     "A film about a boy adrift at sea with a Bengal tiger.",        "The tiger is named Richard Parker.",                        "", "The tiger was almost entirely computer-generated.",            "HARD"   },
    { "UP",          "MOVIES",     "An animated film about an old man who flies his house with balloons.","A young scout named Russell tags along.",             "", "It took 20,622 balloons to lift the house in the film.",       "EASY"   },
    { "WALLE",       "MOVIES",     "An animated film about a lonely robot cleaning up Earth.",     "He falls in love with a robot named EVE.",                  "", "The robot barely speaks for the film's first half.",           "MEDIUM" },
    { "BRAVE",       "MOVIES",     "An animated film about a Scottish princess with wild red hair.","She is a skilled archer.",                                 "", "Merida's hair has more than 1,500 individual curls.",          "MEDIUM" },
    { "LUCA",        "MOVIES",     "An animated film about sea monsters spending a summer in Italy.","They turn human when dry.",                              "", "It is set on the Italian Riviera in the 1950s.",               "MEDIUM" },
    { "SOUL",        "MOVIES",     "An animated film about a jazz musician who ends up in the afterlife.","He must find his way back to Earth.",                "", "It was Pixar's first film with a Black lead character.",       "MEDIUM" },
    { "TURNINGRED",  "MOVIES",     "An animated film about a girl who turns into a giant red panda.","It happens whenever she gets excited.",                  "", "It is set in Toronto in 2002.",                                "HARD"   },
    { "ELEMENTAL",   "MOVIES",     "An animated film set in a city where fire, water, land and air live together.","A fire girl and a water boy fall in love.",  "", "It was inspired by the director's immigrant parents.",         "HARD"   },
    { "WISH",        "MOVIES",     "An animated film about a girl who wishes on a star that answers.","It celebrates Disney's 100th birthday.",                 "", "It contains references to dozens of classic Disney films.",    "MEDIUM" },
    { "TROLLS",      "MOVIES",     "An animated film about tiny colourful creatures with tall hair.","They sing and hug every hour.",                            "", "It is based on the popular troll dolls.",                      "EASY"   },
    { "SPIRIT",      "MOVIES",     "An animated film about a wild mustang who refuses to be tamed.","It is set in the American Old West.",                      "", "The horse never speaks in the film.",                          "MEDIUM" },
    { "ICEAGE",      "MOVIES",     "An animated film about a mammoth, a sloth and a tiger protecting a baby.","A squirrel chases an acorn throughout.",         "", "The squirrel Scrat became the series' most popular character.", "EASY"  },
    { "SHARKTALE",   "MOVIES",     "An animated film about a fish who pretends to be a shark slayer.","Will Smith voices the fish Oscar.",                      "", "The fish characters were designed to resemble their voice actors.", "HARD" },
    { "RIO",         "MOVIES",     "An animated film about a rare blue macaw who cannot fly.",     "It takes place during the Rio carnival.",                   "", "Its main character is a Spix's macaw, now extinct in the wild.", "MEDIUM" },
    { "HAPPYFEET",   "MOVIES",     "An animated film about a penguin who dances instead of singing.","He is named Mumble.",                                     "", "It won the Oscar for best animated feature in 2007.",          "MEDIUM" },
    { "CHICKENRUN",  "MOVIES",     "A stop-motion film about chickens plotting an escape from a farm.","A rooster named Rocky helps them.",                     "", "It is the highest-grossing stop-motion film ever.",            "HARD"   },
    { "CORALINE",    "MOVIES",     "A stop-motion film about a girl who finds a secret door to another world.","Her other mother has buttons for eyes.",        "", "It took four years to animate.",                               "HARD"   },
    { "GHOSTBUSTERS","MOVIES",     "A comedy about scientists who catch ghosts in New York.",      "Who you gonna call?",                                       "", "Its theme song topped the charts for three weeks.",            "HARD"   },
    { "GREMLINS",    "MOVIES",     "A comedy horror about cute creatures that turn nasty if fed after midnight.","Never get them wet.",                        "", "The film led to the creation of the PG-13 rating.",            "HARD"   },
    { "BEETLEJUICE", "MOVIES",     "A comedy about a mischievous ghost summoned by saying his name three times.","He wears a black and white striped suit.",   "", "The title character appears for only 17 minutes.",             "HARD"   },
    { "HOOK",        "MOVIES",     "A film about a grown-up Peter Pan returning to Neverland.",    "Robin Williams plays Peter.",                               "", "The set of the pirate ship cost millions to build.",           "MEDIUM" },
    { "ELF",         "MOVIES",     "A Christmas comedy about a human raised by Santa's elves.",    "He loves maple syrup on spaghetti.",                        "", "Will Ferrell ate real sugary spaghetti in the film.",          "EASY"   },
    { "GRINCH",      "MOVIES",     "A film about a green grump who tries to steal Christmas.",     "His heart grows three sizes.",                              "", "Jim Carrey's makeup took eight hours each day.",               "EASY"   },
    { "POLAREXPRESS","MOVIES",     "An animated Christmas film about a magical train to the North Pole.","Tom Hanks plays several characters.",                 "", "It was the first film made entirely with motion capture.",     "HARD"   },
    { "SCROOGE",     "MOVIES",     "A Christmas film about a miser visited by three ghosts.",      "It is based on Dickens' A Christmas Carol.",                "", "The story has been filmed more than 100 times.",               "MEDIUM" },
    { "CASPER",      "MOVIES",     "A film about a friendly ghost who befriends a young girl.",    "He lives in a haunted mansion.",                            "", "It was the first film with a fully computer-animated lead character.", "MEDIUM" },
    { "SPEED",       "MOVIES",     "A thriller about a bus that will explode if it slows down.",   "Keanu Reeves must keep it above 50 mph.",                   "", "A real bus was launched over a 50-foot gap for one scene.",    "EASY"   },
    { "TWISTER",     "MOVIES",     "A disaster film about scientists chasing tornadoes.",          "A flying cow is its most famous scene.",                    "", "It was the first film released on DVD in the United States.",  "MEDIUM" },
    { "TITAN",       "MOVIES",     "An animated sci-fi film about humanity's last hope after Earth is destroyed.","Its full title mentions the year 3000.",  "", "It mixed traditional animation with early CGI.",               "HARD"   },

    /* =============================== GENERAL =========================== */
    { "MOUNTAIN",    "GENERAL",    "A very high landform rising far above the land around it.",    "Mount Everest is the highest one.",                        "", "Everest grows about four millimetres every year.",            "EASY"   },
    { "OCEAN",       "GENERAL",    "A vast body of salt water that covers most of the planet.",    "There are five of them.",                                  "", "More than eighty percent of it is unexplored.",                "EASY"   },
    { "SUNFLOWER",   "GENERAL",    "A tall yellow flower that turns to follow the sun.",           "Its seeds are pressed for cooking oil.",                   "", "Young ones track the sun from east to west all day.",         "EASY"   },
    { "RAINBOW",     "GENERAL",    "A colourful arc that appears in the sky after rain.",          "It always shows seven colours.",                           "", "You can never actually reach its end.",                       "EASY"   },
    { "LIBRARY",     "GENERAL",    "A quiet place full of books that you may borrow.",             "It has tall shelves and a silence rule.",                  "", "The oldest working one is over a thousand years old.",        "EASY"   },
    { "ISLAND",      "GENERAL",    "A piece of land completely surrounded by water.",              "It can be tropical or covered in ice.",                    "", "Greenland is the largest one on Earth.",                      "EASY"   },
    { "DESERT",      "GENERAL",    "A very dry region that receives very little rainfall.",        "The Sahara is the largest hot one.",                       "", "Some of them are freezing cold all year.",                    "EASY"   },
    { "FOREST",      "GENERAL",    "A large area of land covered with trees.",                     "The Amazon is the largest one.",                            "", "They produce a large share of our oxygen.",                    "EASY"   },
    { "RIVER",       "GENERAL",    "A long stream of water that flows towards the sea.",           "The Nile is the longest one.",                             "", "Rivers slowly shape the land they cross.",                    "EASY"   },
    { "BRIDGE",      "GENERAL",    "A structure built to cross a river, valley or road.",          "It can be made of steel, stone or wood.",                 "", "The longest one in the world is over 164 kilometres.",        "EASY"   },
    { "MAP",         "GENERAL",    "A drawing that shows where places are located.",               "It helps you find your way.",                              "", "North is usually drawn at the top.",                          "EASY"   },
    { "GLOBE",       "GENERAL",    "A spherical model of the planet Earth.",                       "It stands on a desk and can spin.",                        "", "The oldest surviving one was made in 1492.",                  "EASY"   },
    { "TELEPHONE",   "GENERAL",    "A device invented so people could talk over long distances.",  "Alexander Graham Bell patented it.",                       "", "The first call ever made was Mr Watson, come here.",          "MEDIUM" },
    { "PYRAMID",     "GENERAL",    "A giant ancient tomb with a square base and sloping sides.",   "The most famous ones stand at Giza.",                      "", "The Great Pyramid uses about 2.3 million stone blocks.",      "MEDIUM" },
    { "VOLCANO",     "GENERAL",    "A mountain that can erupt with lava, ash and smoke.",          "Its name comes from the Roman god of fire.",              "", "There are roughly 1,500 active ones on Earth.",               "MEDIUM" },
    { "COMPASS",     "GENERAL",    "A tool with a needle that always points to the north.",        "Sailors use it to find their way.",                        "", "It was invented in ancient China.",                            "MEDIUM" },
    { "TREASURE",    "GENERAL",    "A store of gold or valuables that has been hidden away.",      "Pirates search for it with old maps.",                    "", "Real pirate chests are extremely rare.",                      "MEDIUM" },
    { "WATERFALL",   "GENERAL",    "A place where a river falls steeply over a cliff.",            "Niagara is a very famous one.",                            "", "Angel Falls is the highest in the world.",                    "MEDIUM" },
    { "LANTERN",    "GENERAL",     "A portable light protected inside a case.",                    "It is carried while camping.",                             "", "Flying ones are released at festivals in Asia.",              "MEDIUM" },
    { "ANCHOR",      "GENERAL",    "A heavy metal object that holds a ship in place.",             "It is dropped from the side of a boat.",                  "", "Its shape is also a symbol of hope.",                          "MEDIUM" },
    { "CASTLE",      "GENERAL",    "A large fortified building from medieval times.",              "Kings and knights once lived in it.",                      "", "Some were protected by moats and drawbridges.",                "MEDIUM" },
    { "WINDMILL",    "GENERAL",    "A structure with sails that are turned by the wind.",          "It was traditionally used to grind grain.",               "", "The Netherlands is famous for them.",                         "MEDIUM" },
    { "TELESCOPE",   "GENERAL",    "An instrument that makes very distant objects appear close.",  "Galileo used one to see Jupiter's moons.",                "", "Space ones orbit above the atmosphere.",                      "MEDIUM" },
    { "MICROSCOPE",  "GENERAL",    "An instrument used for viewing extremely tiny things.",        "It reveals cells and bacteria.",                           "", "It opened up the whole field of microbiology.",                "MEDIUM" },
    { "CALENDAR",    "GENERAL",    "A system for dividing and naming the days of the year.",       "It shows the months and their dates.",                    "", "Leap years add one extra day every four years.",               "MEDIUM" },
    { "DICTIONARY",  "GENERAL",    "A reference book that lists words and their meanings.",        "It is arranged from A to Z.",                              "", "The Oxford one took seventy years to complete.",              "MEDIUM" },
    { "GLACIER",     "GENERAL",    "A huge, slow-moving mass of ice.",                             "It carves out valleys as it moves.",                       "", "They hold most of the Earth's fresh water.",                  "HARD"   },
    { "CANYON",      "GENERAL",    "A deep valley with very steep rocky sides.",                   "A famous one is in Arizona.",                              "", "The Grand Canyon is 1.8 kilometres deep.",                    "HARD"   },
    { "CAVERN",      "GENERAL",    "A large natural chamber hidden underground.",                  "Stalactites hang down inside it.",                         "", "Some of them contain ancient cave paintings.",                "HARD"   },
    { "LIGHTHOUSE",  "GENERAL",    "A tower that warns ships with a powerful light.",              "It stands on a rocky coast.",                              "", "Some of their beams reach thirty kilometres.",                 "HARD"   },
    { "OBSERVATORY", "GENERAL",    "A building used for watching the stars and planets.",          "It usually holds a large telescope.",                      "", "The best ones are built high on mountains.",                  "HARD"   },
    { "ENCYCLOPEDIA","GENERAL",    "A book or set of books covering every branch of knowledge.",   "It is arranged in alphabetical order.",                   "", "The first one was written in ancient Rome.",                  "HARD"   },
    { "SCHOOL",      "GENERAL",    "A place where children go to learn.",                          "It has classrooms, teachers and a playground.",            "", "The oldest school still open is in England, founded in 597.",  "EASY"   },
    { "TEACHER",     "GENERAL",    "A person who helps students learn.",                           "They stand at the front of the classroom.",                 "", "World Teachers' Day is celebrated on 5 October.",              "EASY"   },
    { "HOSPITAL",    "GENERAL",    "A building where sick and injured people are treated.",        "Doctors and nurses work there.",                            "", "The first hospitals were built more than 2,000 years ago.",    "EASY"   },
    { "DOCTOR",      "GENERAL",    "A person trained to treat sick people.",                       "You visit one when you feel ill.",                          "", "It takes about ten years of study to become one.",             "EASY"   },
    { "GARDEN",      "GENERAL",    "A piece of land where flowers and vegetables are grown.",      "It needs water and sunshine.",                              "", "Gardening burns as many calories as a gym workout.",           "EASY"   },
    { "KITCHEN",     "GENERAL",    "The room of a house where food is cooked.",                    "It has a stove, a sink and a fridge.",                      "", "The word comes from the Latin for to cook.",                   "EASY"   },
    { "WINDOW",      "GENERAL",    "An opening in a wall filled with glass to let light in.",      "You look through it to see outside.",                       "", "The word comes from an old Norse word meaning wind eye.",      "EASY"   },
    { "PILLOW",      "GENERAL",    "A soft cushion you rest your head on in bed.",                 "It is filled with feathers or foam.",                       "", "Ancient Egyptians slept on pillows made of stone.",            "EASY"   },
    { "BLANKET",     "GENERAL",    "A large piece of soft cloth that keeps you warm in bed.",      "You pull it up on cold nights.",                            "", "It is named after Thomas Blanket, a 14th-century weaver.",     "EASY"   },
    { "UMBRELLA",    "GENERAL",    "A folding shade you hold over your head in the rain.",         "It has a curved handle and spokes.",                        "", "The first ones protected people from the sun, not rain.",      "EASY"   },
    { "BICYCLE",     "GENERAL",    "A two-wheeled vehicle you move by pedalling.",                 "It has handlebars and a bell.",                             "", "There are about one billion bicycles in the world.",           "EASY"   },
    { "AIRPLANE",    "GENERAL",    "A flying machine with wings and engines.",                     "It carries people across oceans.",                          "", "The Wright brothers' first flight lasted twelve seconds.",     "EASY"   },
    { "ROCKET",      "GENERAL",    "A vehicle that blasts into space.",                            "It launches with fire and smoke.",                          "", "A rocket must travel 40,000 km/h to leave Earth.",             "EASY"   },
    { "TRAIN",       "GENERAL",    "A long vehicle that runs on rails.",                           "It stops at stations.",                                     "", "The fastest train in the world reaches 600 km/h.",             "EASY"   },
    { "SUBMARINE",   "GENERAL",    "A ship that travels under the sea.",                           "It has a periscope to see above the water.",                "", "Submarines can stay underwater for months.",                   "MEDIUM" },
    { "HELICOPTER",  "GENERAL",    "An aircraft that flies using spinning blades.",                "It can hover in one place.",                                "", "Leonardo da Vinci sketched one 500 years ago.",                "MEDIUM" },
    { "TRACTOR",     "GENERAL",    "A strong farm vehicle with huge back wheels.",                 "It pulls ploughs through fields.",                          "", "The first tractors were powered by steam.",                    "EASY"   },
    { "GUITAR",      "GENERAL",    "A musical instrument with six strings that you strum.",       "It can be acoustic or electric.",                           "", "The oldest guitar-like instrument is 3,500 years old.",        "EASY"   },
    { "PIANO",       "GENERAL",    "A large musical instrument with black and white keys.",       "Pressing a key makes a hammer hit a string.",               "", "A piano has about 230 strings inside.",                        "EASY"   },
    { "VIOLIN",      "GENERAL",    "A small stringed instrument played with a bow.",              "It rests under the chin.",                                  "", "A single violin can be worth millions of dollars.",            "MEDIUM" },
    { "DRUM",        "GENERAL",    "A hollow instrument you hit to make a beat.",                  "It is the heart of every band.",                            "", "Drums are among the oldest instruments in the world.",         "EASY"   },
    { "TRUMPET",     "GENERAL",    "A shiny brass instrument with three valves.",                  "You blow into it to make a bright sound.",                  "", "Trumpets were found in Tutankhamun's tomb.",                   "MEDIUM" },
    { "FLUTE",       "GENERAL",    "A thin instrument you blow across to make a soft sound.",      "It is held sideways.",                                      "", "The oldest known flute is 40,000 years old.",                  "EASY"   },
    { "ORCHESTRA",   "GENERAL",    "A large group of musicians playing together.",                 "A conductor waves a baton in front of it.",                 "", "A full orchestra can have more than 100 players.",             "HARD"   },
    { "PAINTING",    "GENERAL",    "A picture made with brushes and colour.",                      "The Mona Lisa is a famous one.",                            "", "The oldest cave paintings are 45,000 years old.",              "EASY"   },
    { "SCULPTURE",   "GENERAL",    "A three-dimensional artwork carved or shaped from a material.","Michelangelo's David is a famous one.",                     "", "The Statue of Liberty is the most famous sculpture in America.", "MEDIUM" },
    { "MUSEUM",      "GENERAL",    "A building where interesting objects are displayed.",          "You can see dinosaur bones and old paintings there.",       "", "The Louvre is the most visited museum in the world.",          "EASY"   },
    { "THEATRE",     "GENERAL",    "A place where plays are performed on a stage.",               "The audience sits and watches.",                            "", "The ancient Greeks built the first theatres.",                 "MEDIUM" },
    { "CIRCUS",      "GENERAL",    "A travelling show with acrobats and clowns.",                  "It performs inside a big tent.",                            "", "The first modern circus opened in London in 1768.",            "EASY"   },
    { "FESTIVAL",    "GENERAL",    "A special celebration with music, food and fun.",              "Diwali and Christmas are examples.",                        "", "The word comes from the Latin word for feast.",                "EASY"   },
    { "BIRTHDAY",    "GENERAL",    "The yearly celebration of the day you were born.",             "You blow out candles on a cake.",                           "", "The most common birthday in the world is in September.",       "EASY"   },
    { "HOLIDAY",     "GENERAL",    "A special day off from school or work.",                       "Families often travel during one.",                         "", "The word comes from holy day.",                                "EASY"   },
    { "BREAKFAST",   "GENERAL",    "The first meal of the day.",                                   "It breaks your overnight fast.",                            "", "Cereal was invented as a health food in the 1800s.",           "EASY"   },
    { "CHOCOLATE",   "GENERAL",    "A sweet brown treat made from cocoa beans.",                   "It melts in your mouth.",                                   "", "The Aztecs used cocoa beans as money.",                        "EASY"   },
    { "SANDWICH",    "GENERAL",    "Food placed between two slices of bread.",                     "It is named after an English earl.",                        "", "The earl wanted to eat without leaving his card game.",        "EASY"   },
    { "PIZZA",       "GENERAL",    "A flat round bread topped with tomato and cheese.",            "It is baked in a hot oven and cut into slices.",            "", "Americans eat about 350 slices every second.",                 "EASY"   },
    { "NOODLES",     "GENERAL",    "Long thin strips of dough eaten in soup or stir-fry.",         "You slurp them from a bowl.",                               "", "The oldest noodles found were 4,000 years old.",               "EASY"   },
    { "HONEY",       "GENERAL",    "A sweet golden liquid made by bees.",                          "It is spread on toast.",                                    "", "Honey found in ancient tombs is still edible.",                "EASY"   },
    { "CHEESE",      "GENERAL",    "A food made from milk that can be soft or hard.",              "Mice love it in cartoons.",                                 "", "There are more than 1,800 kinds of cheese.",                   "EASY"   },
    { "BUTTER",      "GENERAL",    "A yellow spread made by churning cream.",                      "It melts on hot toast.",                                    "", "It takes about ten litres of milk to make half a kilo of it.", "EASY"   },
    { "BAKERY",      "GENERAL",    "A shop that makes and sells bread and cakes.",                 "It smells wonderful in the morning.",                       "", "Bread has been baked for at least 14,000 years.",              "EASY"   },
    { "RESTAURANT",  "GENERAL",    "A place where you pay to eat a meal.",                         "A waiter brings you the menu.",                             "", "The first restaurants opened in Paris in the 1760s.",          "MEDIUM" },
    { "MARKET",      "GENERAL",    "A place where people buy and sell goods.",                     "Stalls sell fruit, vegetables and clothes.",                "", "Some markets are more than a thousand years old.",             "EASY"   },
    { "MONEY",       "GENERAL",    "Coins and notes used to buy things.",                          "You keep it in a wallet or a bank.",                        "", "The first paper money was used in China 1,000 years ago.",     "EASY"   },
    { "WALLET",      "GENERAL",    "A small folding case for money and cards.",                    "You keep it in your pocket.",                               "", "Wallets became common after paper money was invented.",        "EASY"   },
    { "JEWELLERY",   "GENERAL",    "Decorative items like rings and necklaces.",                   "They are made of gold, silver and gems.",                   "", "The oldest jewellery is 100,000-year-old shell beads.",        "HARD"   },
    { "DIAMOND",     "GENERAL",    "The hardest natural material and a precious gem.",             "It sparkles on engagement rings.",                          "", "Diamonds are made of pure carbon, like pencil lead.",          "MEDIUM" },
    { "CRYSTAL",     "GENERAL",    "A clear solid with a regular shape, like ice or quartz.",      "Snowflakes are made of tiny ones.",                         "", "Every snowflake crystal has six sides.",                       "MEDIUM" },
    { "MAGNET",      "GENERAL",    "An object that pulls iron towards it.",                        "You stick notes on the fridge with one.",                   "", "The Earth itself is a giant magnet.",                          "EASY"   },
    { "BALLOON",     "GENERAL",    "A rubber bag that swells when filled with air or gas.",        "It floats away if filled with helium.",                     "", "The first rubber balloons were made in 1824.",                 "EASY"   },
    { "CANDLE",      "GENERAL",    "A stick of wax with a wick that gives light when lit.",        "You blow them out on a cake.",                              "", "Candles were once made from whale fat.",                       "EASY"   },
    { "MIRROR",      "GENERAL",    "A shiny surface that shows your reflection.",                  "You look into it when you comb your hair.",                 "", "The first mirrors were pools of dark water.",                  "EASY"   },
    { "CLOCK",       "GENERAL",    "A device that shows the time with hands or digits.",           "It ticks on the wall.",                                     "", "The first mechanical clocks had no faces, only bells.",        "EASY"   },
    { "HOURGLASS",   "GENERAL",    "Two glass bulbs with sand flowing from one to the other.",     "It measures time as the sand runs out.",                    "", "Sailors used them to time their watches at sea.",              "MEDIUM" },
    { "LADDER",      "GENERAL",    "A set of steps used to climb up to high places.",              "Firefighters carry a long one.",                            "", "Walking under one is considered bad luck.",                    "EASY"   },
    { "HAMMER",      "GENERAL",    "A tool with a heavy head used to hit nails.",                  "It lives in every toolbox.",                                "", "Hammers are more than three million years old.",               "EASY"   },
    { "SCISSORS",    "GENERAL",    "A cutting tool with two blades that open and close.",          "You use it to cut paper.",                                  "", "Leonardo da Vinci is sometimes wrongly credited with inventing them.", "EASY" },
    { "PENCIL",      "GENERAL",    "A writing tool with a graphite core you can erase.",           "You sharpen it when the tip breaks.",                       "", "One pencil can draw a line 56 kilometres long.",               "EASY"   },
    { "NOTEBOOK",    "GENERAL",    "A book of blank pages for writing notes.",                     "Students carry it to class.",                               "", "The spiral binding was invented in 1924.",                     "EASY"   },
    { "ENVELOPE",    "GENERAL",    "A paper cover that holds a letter for posting.",               "You write the address on the front.",                       "", "The first envelopes were made of clay in Babylon.",            "MEDIUM" },
    { "STAMP",       "GENERAL",    "A small sticky paper you put on a letter to post it.",         "Collectors keep rare ones in albums.",                      "", "The first postage stamp was the Penny Black of 1840.",         "EASY"   },
    { "NEWSPAPER",   "GENERAL",    "Large printed sheets that report the day's news.",             "It is delivered every morning.",                            "", "The oldest newspaper still printed began in 1645.",            "MEDIUM" },
    { "MAGAZINE",    "GENERAL",    "A glossy publication with articles and pictures.",             "It comes out weekly or monthly.",                           "", "The first magazine was published in Germany in 1663.",         "MEDIUM" },
    { "ALPHABET",    "GENERAL",    "The set of letters used to write a language.",                 "English has 26 of them.",                                   "", "The word comes from the first two Greek letters.",             "MEDIUM" },
    { "LANGUAGE",    "GENERAL",    "A system of words people use to communicate.",                 "English, Hindi and Spanish are examples.",                  "", "There are more than 7,000 languages spoken today.",            "MEDIUM" },
    { "POETRY",      "GENERAL",    "Writing that uses rhythm and often rhyme to express feelings.","Shakespeare wrote sonnets, a kind of it.",                  "", "The oldest known poem is 4,000 years old.",                    "MEDIUM" },
    { "HISTORY",     "GENERAL",    "The study of past events.",                                    "It is full of kings, wars and inventions.",                 "", "Written history began about 5,000 years ago.",                 "EASY"   },
    { "SCIENCE",     "GENERAL",    "The study of the natural world through experiments.",          "Physics, chemistry and biology are branches.",              "", "The word scientist was first used in 1833.",                   "EASY"   },
    { "GEOGRAPHY",   "GENERAL",    "The study of the Earth's lands, oceans and people.",           "Maps are its main tool.",                                   "", "The word means writing about the Earth in Greek.",             "MEDIUM" },
    { "MATHEMATICS", "GENERAL",    "The study of numbers, shapes and patterns.",                   "Adding and multiplying are part of it.",                    "", "Zero was invented in India.",                                  "HARD"   },
    { "ASTRONOMY",   "GENERAL",    "The study of stars, planets and space.",                       "Telescopes are its main tool.",                             "", "It is the oldest of the natural sciences.",                    "HARD"   },
    { "PLANET",      "GENERAL",    "A large round body that orbits a star.",                       "Earth is one of eight in our solar system.",                "", "Venus spins the opposite way to most planets.",                "EASY"   },
    { "SATURN",      "GENERAL",    "The planet famous for its beautiful rings.",                   "It is the sixth planet from the Sun.",                      "", "Saturn is so light it would float in water.",                  "MEDIUM" },
    { "JUPITER",     "GENERAL",    "The largest planet in our solar system.",                      "It has a giant red spot storm.",                            "", "More than 1,300 Earths could fit inside it.",                  "MEDIUM" },
    { "GALAXY",      "GENERAL",    "A huge group of billions of stars held together by gravity.",  "Ours is called the Milky Way.",                             "", "There are about two trillion galaxies in the universe.",       "MEDIUM" },
    { "COMET",       "GENERAL",    "An icy space object with a glowing tail.",                     "Halley's is the most famous one.",                          "", "A comet's tail always points away from the Sun.",              "MEDIUM" },
    { "METEOR",      "GENERAL",    "A streak of light from space rock burning in the atmosphere.", "People call it a shooting star.",                           "", "About 25 million of them hit the atmosphere every day.",       "MEDIUM" },
    { "ECLIPSE",     "GENERAL",    "When the Moon blocks the Sun or the Earth's shadow covers the Moon.","Day turns to night for a few minutes.",               "", "A total solar eclipse happens somewhere every 18 months.",     "MEDIUM" },
    { "ASTRONAUT",   "GENERAL",    "A person trained to travel into space.",                       "They float inside the space station.",                      "", "Astronauts grow up to five centimetres taller in space.",      "MEDIUM" },
    { "GRAVITY",     "GENERAL",    "The invisible force that pulls things towards the ground.",    "It is why apples fall from trees.",                         "", "Gravity on the Moon is one sixth of Earth's.",                 "MEDIUM" },
    { "OXYGEN",      "GENERAL",    "The gas in the air that we need to breathe.",                  "Plants make it during the day.",                            "", "Oxygen makes up about 21 percent of the air.",                 "MEDIUM" },
    { "THUNDER",     "GENERAL",    "The loud rumbling sound that follows lightning.",              "Count the seconds to know how far the storm is.",           "", "Thunder is the sound of air expanding at supersonic speed.",   "EASY"   },
    { "LIGHTNING",   "GENERAL",    "A bright flash of electricity in the sky during a storm.",     "It is hotter than the surface of the Sun.",                 "", "Lightning strikes the Earth about 100 times every second.",    "MEDIUM" },
    { "TORNADO",     "GENERAL",    "A violent spinning column of air reaching down from a cloud.", "It is also called a twister.",                              "", "Tornado winds can exceed 480 km/h.",                           "MEDIUM" },
    { "HURRICANE",   "GENERAL",    "A huge spinning storm that forms over warm oceans.",           "It has a calm centre called the eye.",                      "", "Hurricanes are named in alphabetical order each year.",        "MEDIUM" },
    { "EARTHQUAKE",  "GENERAL",    "A sudden shaking of the ground caused by moving rock.",        "It is measured on the Richter scale.",                      "", "About 500,000 earthquakes happen every year.",                 "MEDIUM" },
    { "TSUNAMI",     "GENERAL",    "A giant ocean wave caused by an underwater earthquake.",       "It can travel as fast as a jet plane.",                     "", "The word is Japanese for harbour wave.",                       "MEDIUM" },
    { "AVALANCHE",   "GENERAL",    "A huge mass of snow sliding rapidly down a mountain.",         "A loud noise can sometimes trigger one.",                   "", "Avalanches can reach 130 km/h within five seconds.",           "HARD"   },
    { "SNOWFLAKE",   "GENERAL",    "A tiny six-sided crystal of ice that falls from the sky.",     "No two are exactly alike.",                                 "", "Some snowflakes have up to 200 crystals joined together.",     "EASY"   },
    { "SUNRISE",     "GENERAL",    "The moment the Sun appears above the horizon in the morning.", "The sky glows orange and pink.",                            "", "The Sun you see rising actually rose eight minutes earlier.",  "EASY"   },
    { "SUNSET",      "GENERAL",    "The moment the Sun disappears below the horizon in the evening.","Photographers love its golden light.",                    "", "Sunsets on Mars are blue.",                                    "EASY"   },
    { "HORIZON",     "GENERAL",    "The line where the sky seems to meet the land or sea.",        "Ships disappear over it.",                                  "", "At sea level the horizon is about five kilometres away.",      "MEDIUM" },
    { "MEADOW",      "GENERAL",    "A field of grass and wild flowers.",                           "Cows and butterflies love it.",                             "", "A single meadow can hold more than 100 plant species.",        "MEDIUM" },
    { "JUNGLE",      "GENERAL",    "A thick tropical forest full of wild animals.",                "Tarzan lives in one.",                                      "", "Jungles cover only six percent of Earth but hold half its species.", "EASY" },
    { "SWAMP",       "GENERAL",    "Wet, muddy land with trees and still water.",                  "Crocodiles and frogs live there.",                          "", "Swamps clean water like giant natural filters.",               "EASY"   },
    { "PRAIRIE",     "GENERAL",    "A vast flat grassland with few trees.",                        "Buffalo once roamed it in America.",                        "", "Prairie grass roots can reach three metres deep.",             "HARD"   },
    { "OASIS",       "GENERAL",    "A green area with water in the middle of a desert.",           "Travellers rest under its palm trees.",                     "", "Some desert cities grew around a single one.",                 "MEDIUM" },
    { "HARBOUR",     "GENERAL",    "A sheltered place where ships can safely anchor.",             "Fishing boats rest there at night.",                        "", "Sydney has one of the largest natural harbours in the world.", "MEDIUM" },
    { "TUNNEL",      "GENERAL",    "A passage dug through a hill or under water.",                 "Trains and cars pass through it.",                          "", "The Channel Tunnel runs 50 kilometres under the sea.",         "EASY"   },
    { "FOUNTAIN",    "GENERAL",    "A structure that shoots water into the air for decoration.",   "People throw coins into it for luck.",                      "", "The Trevi Fountain collects about 3,000 euros a day.",         "MEDIUM" },
    { "STATUE",      "GENERAL",    "A carved or cast figure of a person or animal.",               "Famous ones stand in city squares.",                        "", "The tallest statue in the world is in India, 182 metres high.", "EASY"  },
    { "PALACE",      "GENERAL",    "A grand home of a king, queen or president.",                  "Buckingham is a famous one.",                               "", "Buckingham Palace has 775 rooms.",                             "EASY"   },
    { "TEMPLE",      "GENERAL",    "A building where people worship.",                             "Many have tall towers and carved stone.",                   "", "The largest temple in the world is Angkor Wat.",               "EASY"   },
    { "SKYSCRAPER",  "GENERAL",    "A very tall building with many floors.",                       "Cities are full of them.",                                  "", "The tallest one is over 828 metres high.",                     "HARD"   },
    { "ELEVATOR",    "GENERAL",    "A moving box that carries people up and down a building.",     "You press a button for your floor.",                        "", "Elevators are the safest form of transport in the world.",     "MEDIUM" },
    { "ESCALATOR",   "GENERAL",    "A moving staircase found in malls and stations.",              "You stand still and it carries you up.",                    "", "The first one was a ride at Coney Island in 1896.",            "MEDIUM" },
    { "PASSPORT",    "GENERAL",    "A small booklet that lets you travel to other countries.",     "It has your photo and gets stamped.",                       "", "Passports were first used in the 1400s.",                      "MEDIUM" },
    { "SUITCASE",    "GENERAL",    "A box with a handle for carrying clothes when travelling.",    "It rolls on little wheels.",                                "", "Wheeled suitcases were only invented in 1970.",                "EASY"   },
    { "ADVENTURE",   "GENERAL",    "An exciting and sometimes risky journey or experience.",       "Explorers seek it out.",                                    "", "The word comes from the Latin for about to happen.",           "MEDIUM" },
    { "KINGDOM",     "GENERAL",    "A country ruled by a king or queen.",                          "Fairy tales are full of them.",                             "", "There are about 43 monarchies left in the world.",             "EASY"   },
    { "KNIGHT",      "GENERAL",    "A warrior in armour who served a king on horseback.",          "He carried a sword and a shield.",                          "", "A full suit of armour weighed about 25 kilograms.",            "EASY"   },
    { "PIRATE",      "GENERAL",    "A robber who attacks ships at sea.",                           "He may have a parrot and a wooden leg.",                    "", "Pirates really did use the skull and crossbones flag.",        "EASY"   },
    { "DRAGON",      "GENERAL",    "A legendary fire-breathing creature with wings.",              "It guards treasure in stories.",                            "", "Dragons appear in the legends of almost every culture.",       "EASY"   },
    { "UNICORN",     "GENERAL",    "A mythical horse with a single spiral horn.",                  "It is the national animal of Scotland.",                    "", "Unicorn horns sold in the Middle Ages were narwhal tusks.",    "EASY"   },
    { "MERMAID",     "GENERAL",    "A legendary sea creature, half woman and half fish.",          "She sings to sailors.",                                     "", "Columbus claimed he saw three mermaids on his voyage.",        "EASY"   },
    { "WIZARD",      "GENERAL",    "A person with magical powers in stories.",                     "He often has a long beard and a staff.",                    "", "The word comes from wise.",                                    "EASY"   },
    { "PUZZLE",      "GENERAL",    "A game or problem that tests your thinking.",                  "Jigsaws and crosswords are examples.",                      "", "The first jigsaw puzzle was a cut-up map in 1767.",            "EASY"   },
    { "RIDDLE",      "GENERAL",    "A tricky question with a clever answer.",                      "The Sphinx asked a famous one.",                            "", "Riddles are among the oldest forms of word play.",             "EASY"   },
    { "SECRET",      "GENERAL",    "Something you keep hidden from other people.",                 "You whisper it to a friend.",                               "", "Most people cannot keep one for more than 47 hours.",          "EASY"   },
    { "SHADOW",      "GENERAL",    "The dark shape made when something blocks the light.",         "It follows you on a sunny day.",                            "", "Your shadow is shortest at midday.",                           "EASY"   },
    { "ECHO",        "GENERAL",    "A sound that bounces back to you off a wall or cliff.",        "You hear it in caves and canyons.",                         "", "The longest echo ever recorded lasted 112 seconds.",           "EASY"   },
    { "WHISPER",     "GENERAL",    "Speaking very quietly using your breath.",                     "You do it in a library.",                                   "", "Whispering strains the voice more than talking.",              "EASY"   },
    { "FRIENDSHIP",  "GENERAL",    "The close bond between people who care for each other.",       "It is celebrated on a special day each August.",            "", "Good friends can add years to your life.",                     "MEDIUM" },
    { "KINDNESS",    "GENERAL",    "The quality of being caring and helpful to others.",           "A small act of it can change someone's day.",               "", "Being kind releases feel-good chemicals in your brain.",       "MEDIUM" },
    { "COURAGE",     "GENERAL",    "Doing something even though it frightens you.",                "Heroes have plenty of it.",                                 "", "The word comes from the Latin for heart.",                     "MEDIUM" },
    { "WISDOM",      "GENERAL",    "Deep understanding gained from experience.",                   "Owls are said to have it.",                                 "", "Wisdom teeth are named because they appear in adulthood.",     "MEDIUM" },
    { "IMAGINATION", "GENERAL",    "The power to picture things that are not there.",              "Children have plenty of it.",                               "", "Einstein said it is more important than knowledge.",           "HARD"   },
    { "CELEBRATION", "GENERAL",    "A happy event marking a special occasion.",                    "Cake, music and friends are part of it.",                   "", "The biggest one on Earth is Chinese New Year.",                "HARD"   },
    { "VACATION",    "GENERAL",    "A period of time away from work or school to relax.",          "Beaches are a popular destination.",                        "", "The word comes from the Latin for freedom.",                   "MEDIUM" },
    { "PHOTOGRAPH",  "GENERAL",    "A picture made with a camera.",                                "You keep them in an album or on your phone.",               "", "About 1.8 trillion photos are taken every year.",              "MEDIUM" },
    { "TROPHY",      "GENERAL",    "A cup or statue given to the winner of a competition.",        "It is usually gold or silver.",                             "", "The FIFA World Cup trophy is made of solid gold.",             "EASY"   },
    { "MEDAL",       "GENERAL",    "A metal disc awarded for winning or bravery.",                 "Gold, silver and bronze are the Olympic ones.",             "", "Olympic gold medals are mostly made of silver.",               "EASY"   },
    { "CHAMPION",    "GENERAL",    "The person or team that wins a competition.",                  "They lift the trophy at the end.",                          "", "The word once meant a fighter who battled for someone else.",  "MEDIUM" },
    { "KEYBOARD",    "GENERAL",    "A set of keys used to type or to play music.",                 "It sits in front of your computer or under a pianist's hands.","", "The QWERTY layout was designed to stop typewriter jams.",     "EASY"   },
    { "SPECTACLES",  "GENERAL",    "Two lenses in a frame worn to see better.",                    "They are also called glasses.",                             "", "Spectacles were invented in Italy in the 1280s.",              "HARD"   },
    { "PERFUME",     "GENERAL",    "A pleasant-smelling liquid you spray on your skin.",           "It comes in fancy bottles.",                                "", "The oldest perfume factory is 4,000 years old, in Cyprus.",    "MEDIUM" },
    { "SOAP",        "GENERAL",    "A bar or liquid used with water to wash things clean.",        "It makes bubbles.",                                         "", "Soap has been made for about 5,000 years.",                    "EASY"   },
    { "TOOTHBRUSH",  "GENERAL",    "A small brush used to clean your teeth.",                      "Dentists say to use it twice a day.",                       "", "The first ones had bristles made from pig hair.",              "MEDIUM" },
    { "SHAMPOO",     "GENERAL",    "A liquid soap used to wash your hair.",                        "It makes a lot of foam.",                                   "", "The word comes from the Hindi word for massage.",              "MEDIUM" },
    { "SWEATER",     "GENERAL",    "A warm knitted top worn in cold weather.",                     "Grandmothers love to knit them.",                           "", "It got its name because athletes wore it to sweat.",           "EASY"   },
    { "GLOVES",      "GENERAL",    "Coverings for your hands with a place for each finger.",       "You wear them when it snows.",                              "", "Gloves have been worn since the Ice Age.",                     "EASY"   },
    { "SANDALS",     "GENERAL",    "Open shoes held on by straps.",                                "You wear them in summer.",                                  "", "The oldest sandals ever found are 10,000 years old.",          "EASY"   }
};

const int WORDS_IN_DATABASE = (int)(sizeof(WORD_DATABASE) / sizeof(WORD_DATABASE[0]));

/* --------------------------------------------------------------------------
   3. CONSTRUCTOR + RESET
   -------------------------------------------------------------------------- */
WordHuntGame::WordHuntGame() { resetGame(); }

void WordHuntGame::resetGame() {
    current          = WordData();
    categoryIndex    = 0;
    difficultyLevel  = 2;
    guessedLetters.clear();
    wrongLetters.clear();
    remainingLives   = 6;
    maxLives         = 6;
    score            = 0;
    roundScore       = 0;
    streak           = 0;
    hintsRemaining   = HINTS_PER_WORD;
    wordsCompleted   = 0;
    gameStatus       = "NOT_STARTED";
    lastMessage      = "Welcome to WORD HUNT!";
    revealedHint     = "";
}

void WordHuntGame::resetCategory(int index) {
    if (index < 0 || index >= NUM_CATEGORIES) return;
    usedWords[index].clear();
}

/* --------------------------------------------------------------------------
   4. SETUP
   -------------------------------------------------------------------------- */
bool WordHuntGame::selectCategory(int index) {
    if (index < 0 || index >= NUM_CATEGORIES) return false;
    categoryIndex = index;
    return true;
}

bool WordHuntGame::selectDifficulty(int level) {
    if (level < 1 || level > 3) return false;
    difficultyLevel = level;
    return true;
}

/* all unused words of one category (the no-repeat pool) */
vector<const WordData*> WordHuntGame::availableWords(int category) const {
    vector<const WordData*> available;
    string catName = categoryName(category);
    for (int i = 0; i < WORDS_IN_DATABASE; i++) {
        if (WORD_DATABASE[i].category != catName) continue;
        bool used = false;
        for (int j = 0; j < (int)usedWords[category].size(); j++) {
            if (usedWords[category][j] == WORD_DATABASE[i].word) { used = true; break; }
        }
        if (!used) available.push_back(&WORD_DATABASE[i]);
    }
    return available;
}

/* --------------------------------------------------------------------------
   THE NO-REPEAT WORD SELECTION
     1. take every word of the category
     2. remove the ones already used        -> availableWords
     3. if empty -> category completed
     4. keep only the words of this difficulty (fall back to the rest)
     5. pick one at random and IMMEDIATELY mark it as used
   -------------------------------------------------------------------------- */
const WordData* WordHuntGame::selectRandomWord(unsigned int seed) {
    vector<const WordData*> available = availableWords(categoryIndex);
    if (available.empty()) return NULL;                    /* category done */

    /* difficulty really changes which words are offered */
    vector<const WordData*> pool;
    if (difficultyLevel == 1)      for (int i = 0; i < (int)available.size(); i++) if (available[i]->difficulty == "EASY")   pool.push_back(available[i]);
    else if (difficultyLevel == 2) for (int i = 0; i < (int)available.size(); i++) if (available[i]->difficulty != "HARD")  pool.push_back(available[i]);
    else                           for (int i = 0; i < (int)available.size(); i++) if (available[i]->difficulty != "EASY")  pool.push_back(available[i]);
    if (pool.empty()) pool = available;                    /* never block the game */

    if (seed == 0) seed = (unsigned int)time(NULL);
    srand(seed);
    const WordData* chosen = pool[rand() % pool.size()];

    usedWords[categoryIndex].push_back(chosen->word);      /* mark as used NOW */
    return chosen;
}

bool WordHuntGame::getNextWord(unsigned int seed) {
    const WordData* next = selectRandomWord(seed);
    if (next == NULL) {                                    /* nothing left */
        gameStatus  = "COMPLETED";
        lastMessage = "Congratulations! You completed all words in " + categoryName(categoryIndex) + ".";
        return false;
    }
    current         = *next;
    guessedLetters.clear();
    wrongLetters.clear();
    maxLives        = livesForDifficulty(difficultyLevel);
    remainingLives  = maxLives;
    roundScore      = 0;
    hintsRemaining  = HINTS_PER_WORD;
    revealedHint    = "";
    gameStatus      = "PLAYING";
    lastMessage     = "New " + categoryName(categoryIndex) + " word! You have "
                    + to_string(maxLives) + " lives. Read the clue and start guessing.";
    return true;
}

/* --------------------------------------------------------------------------
   5. THE RULES
   -------------------------------------------------------------------------- */
bool WordHuntGame::isLetterInWord(char letter) const {
    for (int i = 0; i < (int)current.word.length(); i++)
        if (current.word[i] == letter) return true;
    return false;
}

static int countInWord(const string& word, char letter) {
    int n = 0;
    for (int i = 0; i < (int)word.length(); i++) if (word[i] == letter) n++;
    return n;
}

string WordHuntGame::getMaskedWord() const {
    string masked = "";
    for (int i = 0; i < (int)current.word.length(); i++) {
        if (i > 0) masked += " ";
        bool found = false;
        for (int j = 0; j < (int)guessedLetters.size(); j++)
            if (guessedLetters[j] == current.word[i]) { found = true; break; }
        masked += found ? string(1, current.word[i]) : "_";
    }
    return masked;
}

bool WordHuntGame::checkWin() const {
    for (int i = 0; i < (int)current.word.length(); i++) {
        bool found = false;
        for (int j = 0; j < (int)guessedLetters.size(); j++)
            if (guessedLetters[j] == current.word[i]) { found = true; break; }
        if (!found) return false;
    }
    return true;
}

bool WordHuntGame::checkGameOver() const { return remainingLives <= 0; }

int WordHuntGame::calculateScore(int base) {
    return (int)(base * multiplierFor(difficultyLevel) + 0.5);      /* rounded */
}
void WordHuntGame::addPoints(int p)  { roundScore += p; score += p; }
void WordHuntGame::losePoints(int p) {
    roundScore -= p; score -= p;
    if (score < 0) score = 0;                     /* never a negative score */
}

/* --------------------------------------------------------------------------
   6. GAMEPLAY
   -------------------------------------------------------------------------- */
GuessResult WordHuntGame::checkGuess(char letter) {
    letter = (char)toupper((unsigned char)letter);

    if (gameStatus != "PLAYING") {
        lastMessage = (gameStatus == "NOT_STARTED")
            ? "The game has not started yet."
            : "This round is already over. Take the next word!";
        return GUESS_ERROR;
    }
    if (letter < 'A' || letter > 'Z') { lastMessage = "Only letters from A to Z are allowed."; return GUESS_INVALID; }

    for (int i = 0; i < (int)guessedLetters.size(); i++)
        if (guessedLetters[i] == letter) {
            lastMessage = "You already guessed " + string(1, letter) + "!";
            return GUESS_DUPLICATE;
        }
    guessedLetters.push_back(letter);

    if (isLetterInWord(letter)) {
        int times = countInWord(current.word, letter);
        addPoints(calculateScore(POINTS_PER_LETTER * times));
        lastMessage = "Correct! " + string(1, letter) + " appears "
                    + to_string(times) + " time(s). +" + to_string(calculateScore(POINTS_PER_LETTER * times)) + " points.";
        if (checkWin()) { finish(true); return GUESS_WIN; }
        return GUESS_CORRECT;
    }

    wrongLetters.push_back(letter);
    remainingLives--;
    losePoints(WRONG_PENALTY);
    if (checkGameOver()) { finish(false); return GUESS_LOST; }
    lastMessage = "Wrong! " + string(1, letter) + " is not in the word. -"
                + to_string(WRONG_PENALTY) + " points. " + to_string(remainingLives) + " lives left.";
    return GUESS_WRONG;
}

void WordHuntGame::revealLetter(char letter) {
    letter = (char)toupper((unsigned char)letter);
    if (letter < 'A' || letter > 'Z') return;
    for (int i = 0; i < (int)guessedLetters.size(); i++)
        if (guessedLetters[i] == letter) return;         /* already visible */
    guessedLetters.push_back(letter);
}

bool WordHuntGame::useHint() {
    if (gameStatus != "PLAYING") { lastMessage = "No game running right now."; return false; }
    if (hintsRemaining <= 0)     { lastMessage = "No hints left for this word!"; return false; }

    losePoints(HINT_COST);

    /* first hint: an extra clue (a vaguer one on HARD, when provided) */
    if (hintsRemaining == HINTS_PER_WORD) {
        string extra = current.hint1;
        if (difficultyLevel == 3 && current.hint2 != "") extra = current.hint2;
        revealedHint = extra;
        hintsRemaining--;
        lastMessage = "Hint used! -" + to_string(HINT_COST) + " points. An extra clue is now shown.";
        return true;
    }

    /* second hint: reveal one still hidden letter */
    char letter = 0;
    for (int i = 0; i < (int)current.word.length(); i++) {
        bool found = false;
        for (int j = 0; j < (int)guessedLetters.size(); j++)
            if (guessedLetters[j] == current.word[i]) { found = true; break; }
        if (!found) { letter = current.word[i]; break; }
    }
    if (letter == 0) { lastMessage = "Nothing left to reveal!"; return false; }
    revealLetter(letter);
    hintsRemaining--;
    lastMessage = "Hint used! -" + to_string(HINT_COST) + " points. The letter "
                + string(1, letter) + " is part of the word.";
    if (checkWin()) finish(true);
    return true;
}

string WordHuntGame::finish(bool won) {
    if (won) {
        int bonus = calculateScore(WORD_BONUS);
        addPoints(bonus);
        streak++;
        wordsCompleted++;
        gameStatus  = "WON";
        lastMessage = "You found the word! +" + to_string(bonus) + " bonus points.";
        return "WIN";
    }
    streak     = 0;
    gameStatus = "LOST";
    lastMessage = "Game over! The word was " + current.word + ".";
    return "LOSE";
}

/* --------------------------------------------------------------------------
   7. USED-WORD TRACKING  (per category, restored by the browser)
      format:  "FRUITS:MANGO,APPLE;SPORTS:CRICKET"
   -------------------------------------------------------------------------- */
void WordHuntGame::setUsedWords(const string& csv) {
    for (int i = 0; i < NUM_CATEGORIES; i++) usedWords[i].clear();
    size_t pos = 0;
    while (pos < csv.length()) {
        size_t semi = csv.find(';', pos);
        if (semi == string::npos) semi = csv.length();
        string pair = csv.substr(pos, semi - pos);
        size_t colon = pair.find(':');
        if (colon != string::npos) {
            string cat  = pair.substr(0, colon);
            string list = pair.substr(colon + 1);
            for (int i = 0; i < NUM_CATEGORIES; i++) {
                if (categoryName(i) == cat) {
                    string item;
                    stringstream ss(list);
                    while (getline(ss, item, ',')) {
                        if (item.length() > 0) usedWords[i].push_back(item);
                    }
                }
            }
        }
        pos = semi + 1;
    }
}

string WordHuntGame::getUsedWords() const {
    string out = "";
    for (int i = 0; i < NUM_CATEGORIES; i++) {
        if (usedWords[i].empty()) continue;
        if (!out.empty()) out += ";";
        out += categoryName(i) + ":";
        for (int j = 0; j < (int)usedWords[i].size(); j++) {
            if (j) out += ",";
            out += usedWords[i][j];
        }
    }
    return out;
}

int WordHuntGame::wordsRemaining(int index) const { return (int)availableWords(index).size(); }

bool WordHuntGame::categoryCompleted(int index) const { return wordsRemaining(index) == 0; }

string WordHuntGame::completedCategories() const {
    string out = "";
    for (int i = 0; i < NUM_CATEGORIES; i++) {
        if (categoryCompleted(i)) {
            if (!out.empty()) out += ",";
            out += to_string(i);
        }
    }
    return out;
}

/* --------------------------------------------------------------------------
   8. WORD BANK HELPERS
   -------------------------------------------------------------------------- */
int WordHuntGame::categoryCount() { return NUM_CATEGORIES; }

/* The database is written in category blocks, so the names are read in order
   from the first word of every block (cached after the first call).        */
string WordHuntGame::categoryName(int index) {
    if (index < 0 || index >= NUM_CATEGORIES) return "UNKNOWN";
    static string cached[NUM_CATEGORIES] = { "", "", "", "", "", "", "" };
    if (cached[0] == "") {
        int seen = 0;
        string previous = "";
        for (int i = 0; i < WORDS_IN_DATABASE && seen < NUM_CATEGORIES; i++) {
            if (WORD_DATABASE[i].category != previous) {
                previous = WORD_DATABASE[i].category;
                cached[seen++] = previous;
            }
        }
    }
    return cached[index];
}

int WordHuntGame::wordsInCategory(int index) {
    string name = categoryName(index);
    int n = 0;
    for (int i = 0; i < WORDS_IN_DATABASE; i++) if (WORD_DATABASE[i].category == name) n++;
    return n;
}

int    WordHuntGame::livesForDifficulty(int level) { return level == 1 ? 8 : (level == 2 ? 6 : 4); }
double WordHuntGame::multiplierFor(int level)      { return level == 1 ? 1.0 : (level == 2 ? 1.5 : 2.0); }
string WordHuntGame::difficultyName(int level)     { return level == 1 ? "EASY" : (level == 2 ? "MEDIUM" : "HARD"); }

/* --------------------------------------------------------------------------
   9. THE PROTOCOL
   -------------------------------------------------------------------------- */
static vector<string> splitLine(const string& line, char delimiter) {
    vector<string> parts; string item; stringstream ss(line);
    while (getline(ss, item, delimiter)) parts.push_back(item);
    return parts;
}

string WordHuntGame::eventName(GuessResult result) const {
    switch (result) {
        case GUESS_INVALID:   return "INVALID";
        case GUESS_DUPLICATE: return "DUPLICATE";
        case GUESS_CORRECT:   return "CORRECT";
        case GUESS_WRONG:     return "WRONG";
        case GUESS_WIN:       return "WIN";
        case GUESS_LOST:      return "LOSE";
        default:              return "ERROR";
    }
}

string WordHuntGame::buildState(const string& event) {
    string guessed = "", wrong = "";
    for (int i = 0; i < (int)guessedLetters.size(); i++) {
        if (i) guessed += ","; guessed += string(1, guessedLetters[i]); }
    for (int i = 0; i < (int)wrongLetters.size(); i++) {
        if (i) wrong += ","; wrong += string(1, wrongLetters[i]); }

    /* how many words each category holds — the UI shows this on the cards */
    string counts = "";
    for (int i = 0; i < NUM_CATEGORIES; i++) {
        if (i) counts += ",";
        counts += to_string(wordsInCategory(i));
    }

    string out = "";
    out += "CAT|"        + categoryName(categoryIndex) + "\n";
    out += "CLUE|"       + current.clue + "\n";
    out += "HINTTEXT|"   + revealedHint + "\n";
    out += "LEN|"        + to_string((int)current.word.length()) + "\n";
    out += "MASK|"       + getMaskedWord() + "\n";
    out += "GUESSED|"    + guessed + "\n";
    out += "WRONG|"      + wrong + "\n";
    out += "LIVES|"      + to_string(remainingLives) + "\n";
    out += "MAX|"        + to_string(maxLives) + "\n";
    out += "SCORE|"      + to_string(score) + "\n";
    out += "ROUND|"      + to_string(roundScore) + "\n";
    out += "STREAK|"     + to_string(streak) + "\n";
    out += "HINTS|"      + to_string(hintsRemaining) + "\n";
    out += "WORDS|"      + to_string(wordsCompleted) + "\n";
    out += "REMAINING|"  + to_string(wordsRemaining(categoryIndex)) + "\n";
    out += "COUNTS|"     + counts + "\n";
    out += "STATUS|"     + gameStatus + "\n";
    out += "EVENT|"      + event + "\n";
    out += "MESSAGE|"    + lastMessage + "\n";
    out += "DIFF|"       + to_string(difficultyLevel) + "\n";
    out += "DONECATS|"   + completedCategories() + "\n";
    out += "USED|"       + getUsedWords() + "\n";
    if (gameStatus == "WON" || gameStatus == "LOST") out += "WORD|" + current.word + "\n";
    if (gameStatus == "WON")                         out += "FACT|" + current.fact  + "\n";
    return out;
}

string WordHuntGame::handleCommand(const string& command) {

    vector<string> p = splitLine(command, '|');
    string head = p.size() > 0 ? p[0] : "";

    /* ---------- restore the used-word history ---------- */
    if (head == "SETUSED") { setUsedWords(p.size() > 1 ? p[1] : ""); return buildState("READY"); }

    /* ---------- restore a player's progress ---------- */
    if (head == "RESUME") {
        score          = p.size() > 1 ? atoi(p[1].c_str()) : 0;
        streak         = p.size() > 2 ? atoi(p[2].c_str()) : 0;
        wordsCompleted = p.size() > 3 ? atoi(p[3].c_str()) : 0;
        if (score < 0) score = 0;
        lastMessage = "Welcome back! Your progress was restored.";
        return buildState("READY");
    }

    /* ---------- start a round ---------- */
    if (head == "START") {
        int cat  = p.size() > 1 ? atoi(p[1].c_str()) : -1;
        int diff = p.size() > 2 ? atoi(p[2].c_str()) : -1;
        unsigned int seed = p.size() > 3 ? (unsigned int)atol(p[3].c_str()) : 0;

        if (!selectCategory(cat))  { lastMessage = "Please choose a category first.";    return buildState("ERROR"); }
        if (!selectDifficulty(diff)){ lastMessage = "Please select a difficulty first."; return buildState("ERROR"); }

        if (!getNextWord(seed)) return buildState("COMPLETED");   /* every word used */
        return buildState("NEW");
    }

    /* ---------- a guess ---------- */
    if (head == "GUESS") {
        string input = p.size() > 1 ? p[1] : "";
        if (input.empty())      { lastMessage = "Please choose a letter.";      return buildState("INVALID"); }
        if (input.length() > 1) { lastMessage = "One letter at a time please!"; return buildState("INVALID"); }
        return buildState(eventName(checkGuess(input[0])));
    }

    /* ---------- hints ---------- */
    if (head == "HINT") { useHint(); return buildState("HINT"); }

    /* ---------- give up ---------- */
    if (head == "GIVEUP") {
        if (gameStatus != "PLAYING") { lastMessage = "No game running right now."; return buildState("ERROR"); }
        finish(false);
        lastMessage = "You gave up! The word was " + current.word + ".";
        return buildState("LOSE");
    }

    /* ---------- free all words of one category ---------- */
    if (head == "RESETCAT") {
        int cat = p.size() > 1 ? atoi(p[1].c_str()) : -1;
        if (cat < 0 || cat >= NUM_CATEGORIES) { lastMessage = "Unknown category."; return buildState("ERROR"); }
        resetCategory(cat);
        lastMessage = categoryName(cat) + " words are available again.";
        return buildState("RESETCAT");
    }

    /* ---------- reset the whole game ---------- */
    if (head == "RESET") {
        resetGame();
        lastMessage = "New game. Score and streak reset.";
        return buildState("RESET");
    }

    if (head == "STATUS") return buildState("STATUS");

    lastMessage = "Unknown command.";
    return buildState("ERROR");
}

/* --------------------------------------------------------------------------
   10. THE BRIDGE FOR THE BROWSER (WebAssembly export)
   -------------------------------------------------------------------------- */
static WordHuntGame engine;      /* the engine instance used by the browser */
static string       engineReply; /* stays alive while the browser reads it  */

extern "C" const char* wordhunt_command(const char* input) {
    if (input == NULL) return "EVENT|ERROR\nMESSAGE|Empty command.";
    engineReply = engine.handleCommand(string(input));
    return engineReply.c_str();
}

/* --------------------------------------------------------------------------
   11. TERMINAL VERSION — the same engine, no browser needed
   -------------------------------------------------------------------------- */
#ifndef WORDHUNT_NO_MAIN

static string field(const string& block, const string& key) {
    string find = key + "|";
    size_t pos = block.find(find);
    if (pos == string::npos) return "";
    size_t start = pos + find.length();
    size_t end   = block.find('\n', start);
    return block.substr(start, end - start);
}

static void showRound(WordHuntGame& g) {
    string st = g.handleCommand("STATUS");
    cout << "\n  CATEGORY : " << field(st, "CAT") << "   (" << field(st, "DIFF") << ")\n";
    cout << "  CLUE     : " << field(st, "CLUE") << "\n";
    if (field(st, "HINTTEXT") != "") cout << "  HINT     : " << field(st, "HINTTEXT") << "\n";
    cout << "  WORD     : " << field(st, "MASK") << "   (" << field(st, "LEN") << " letters)\n";
    cout << "  TRIED    : " << field(st, "GUESSED") << "\n";
    cout << "  LIVES    : " << field(st, "LIVES") << " / " << field(st, "MAX")
         << "   SCORE: " << field(st, "SCORE")
         << "   STREAK: " << field(st, "STREAK")
         << "   HINTS: " << field(st, "HINTS") << "\n";
    cout << "  Words left in this category: " << field(st, "REMAINING") << "\n";
}

int main() {
    cout << "====================================================\n";
    cout << "              W O R D   H U N T\n";
    cout << "      A Word Guessing Adventure (C++ engine)\n";
    cout << "====================================================\n";

    WordHuntGame game;
    bool again = true;
    while (again) {
        cout << "\nCategories:\n";
        for (int i = 0; i < WordHuntGame::categoryCount(); i++)
            cout << "  " << (i + 1) << ". " << WordHuntGame::categoryName(i)
                 << " (" << WordHuntGame::wordsInCategory(i) << " words)\n";
        int cat = 0;
        cout << "Choose a category (1-7): "; cin >> cat; cat--;
        if (!game.selectCategory(cat)) { cout << "Invalid category.\n"; continue; }

        cout << "\n1. EASY (8 lives)   2. MEDIUM (6 lives)   3. HARD (4 lives)\n";
        int diff = 2;
        cout << "Choose difficulty (1-3): "; cin >> diff;
        if (!game.selectDifficulty(diff)) { cout << "Invalid difficulty.\n"; continue; }

        while (true) {
            if (!game.getNextWord(0)) {
                cout << "\n  Congratulations! You completed all words in this category.\n";
                break;
            }
            while (game.getGameStatus() == "PLAYING") {
                showRound(game);
                cout << "\nGuess a letter (or ? for a hint): ";
                string input; cin >> input;
                if (input == "?") game.useHint();
                else if (input.length() == 1) game.checkGuess(input[0]);
                else cout << "One letter at a time please!\n";
                cout << "\n>> " << field(game.handleCommand("STATUS"), "MESSAGE") << "\n";
            }
            string st = game.handleCommand("STATUS");
            if (game.getGameStatus() == "WON")
                cout << "\n*** WORD COMPLETE ***  " << field(st, "WORD")
                     << "\n    DID YOU KNOW? " << field(st, "FACT") << "\n";
            else if (game.getGameStatus() == "LOST")
                cout << "\n*** GAME OVER ***  The word was " << field(st, "WORD") << "\n";

            cout << "\nNext word in this category? (y/n): ";
            string more; cin >> more;
            if (more != "y" && more != "Y") break;
        }

        cout << "\nPlay again? (y/n): ";
        string ans; cin >> ans;
        again = (ans == "y" || ans == "Y");
        game.resetGame();
    }
    cout << "\nThanks for playing WORD HUNT!\n";
    return 0;
}

#endif /* WORDHUNT_NO_MAIN */
