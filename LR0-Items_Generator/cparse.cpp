
/*
 *  Name: Neet Patel
 *  Class: COP 4020
 *  Assignment: Proj 3(Producing LR(0) items for a certain non-regular, CFG)
 *  Compile: "g++ -c -o cparse.o cparse.cpp -std=c++11
              g++ -o cparse.exe cparse.o -std=c++11"
 */

#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <cctype>
#include <set>
#include <map>
#include <string>
#include <list>
#include <queue>

typedef std::pair<std::string, int> STR_NUM_PAIR; // pair of: string - int -- used to hold a production string with a marker and the position of the char after the marker
typedef std::list<STR_NUM_PAIR> STR_NUM_LIST; // list of STR_NUM_PAIR
typedef std::pair<char, std::list<STR_NUM_PAIR> > CH_STR_NUM_LIST_PAIR; // pair of: char - STR_NUM_LIST

int main() {
    std::map<char, STR_NUM_LIST> closureMap; // mapping of: NT symbols -> closure productions
    std::queue<STR_NUM_LIST, std::list<STR_NUM_LIST> > workQueue; // queue of: lists of items awaiting processing
    std::map<std::string, int> item_to_goto;
    std::pair<std::map<std::string, int >::iterator, bool> itgRetr;
    char front('\0');
    std::string pro(8, '\0'); // allocate size of 8 by default to hold "pro"duction string
    int itemNum = 1;
    // Print augmented grammar
    pro.resize(0); // resize to 0;
    printf("Augmented Grammar\n");
    printf("-----------------\n");
    pro = "\'->"; // initialize to the first production for augmented grammar
    while ((front = std::fgetc(stdin)) != EOF) {
        // read in CFG productions
        while (front != '\n') {
            // add the char to the pro string untill newline
            pro.push_back(front);
            front = std::fgetc(stdin);
        }
        printf("%s\n", pro.c_str()); // print this production
        pro.insert(3, 1, '@'); // add the marker @ symbol to initial position
        closureMap[pro.front()].push_back(STR_NUM_PAIR(pro, 4));
        pro.resize(0);
    }
    // Now compute LR(0) itmes
    printf("\n");
    printf("Sets of LR(0) Items\n");
    printf("-------------------\n");
    workQueue.push(closureMap['\'']); // get the work queue started
    for (int i = 0; ! workQueue.empty(); ++i) {
        std::map < char, STR_NUM_LIST > front_to_newItemList; // hold a symbol mapped to new item list; the item lists will be pushed into work queue
        printf("I%d:\n", i);
        for (auto& j : workQueue.front()) {
            // j ==  production STR_NUM_PAIR from the the current item list
            std::list<char> orderList; // list that will keep the order in which to push the new item lists into the work queue
            front = j.first[j.second];
            if (j.second == j.first.size()) {
                // marker reached end of the production
                printf("   %-20s\n", j.first.c_str());
                continue;
            }
            itgRetr = item_to_goto.insert(STR_NUM_PAIR(j.first, itemNum)); // check if production is already mapped to a goto
            if (itgRetr.second == false) {
                // yes it is mapped
                printf("   %-20s goto(%c)=I%d\n", j.first.c_str(), front, itgRetr.first->second);
            } else {
                // no it isn't mapped
                orderList.push_back(front); // push in the order List
                printf("   %-20s goto(%c)=I%d\n", j.first.c_str(), front, itemNum++);
            }
            // "increment" marker and  append production to the list mapped to this front; the item list will be pushed into work queue(according to the order list)
            std::swap(j.first[j.second], j.first[j.second - 1]);
            front_to_newItemList.insert(CH_STR_NUM_LIST_PAIR(front, STR_NUM_LIST(1, STR_NUM_PAIR(j.first, ++j.second))));
            // get closure if NT symbol; first production already handled from above code
            if (isupper(front)) {
                std::set<char> closureDone; // set holding NT symbols that have been marked to take closure
                std::queue<char, std::list<char> > closureQueue; // queue of NT symbols awaiting closure
                bool uniqueNT;
                closureDone.insert(front); // push NT in the closureDone set
                closureQueue.push(front); // push NT in queue for getting closure
                while (!closureQueue.empty()) {
                    for (auto k: closureMap[closureQueue.front()]) {
                        // k == production STR_NUM_PAIR from closure list from closure map
                        front = k.first[k.second];
                        uniqueNT = false;
                        if (closureDone.find(front) == closureDone.end()) {
                            // new NT symbol
                            uniqueNT = true;
                            closureDone.insert(front); // add to closure done queue
                            closureQueue.push(front); // push in closure queue
                        }
                        if (uniqueNT || !isupper(front)) {
                            // if new NT symbol or token, check if already mapped to goto
                            itgRetr = item_to_goto.insert(STR_NUM_PAIR(k.first, itemNum));
                            if (itgRetr.second == false) {
                                // already mapped
                                printf("   %-20s goto(%c)=I%d\n", k.first.c_str(), front, itgRetr.first->second);
                            } else { // not mapped
                                orderList.push_back(front); // push in order List
                                printf("   %-20s goto(%c)=I%d\n", k.first.c_str(), front, itemNum++);
                            }
                        } else {
                            // not New NT symbol so just print
                            printf("   %-20s\n", k.first.c_str());
                        }
                        // do the same increment and appending to list mapped to current "front" process
                        std::swap(k.first[k.second], k.first[k.second - 1]);
                        front_to_newItemList[front].push_back(STR_NUM_PAIR(k.first, ++k.second));
                    } // end closureMap for loop
                    closureQueue.pop();
                } // end closueQue while loop
            } // end if NT symbol
            for (auto& l: orderList) {
                //push new item lists in to the queue
                workQueue.push(front_to_newItemList[l]);
            }
        } // end item list for loop
        printf("\n");
        workQueue.pop(); // pop the item list
    } // end workQueue for loop */
    return 0 ;
}
